/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <condition_variable>
#include <curl/curl.h>
#include <functional>
#include <memory>
#include <mutex>

#include "async.hpp"
#include "authentication.hpp"
#include "common.hpp"
#include "options.hpp"

namespace curlev
{

constexpr long c_code_success                    =   0;
constexpr long c_code_error_option               =  -1;
constexpr long c_code_error_http_headers         =  -2;
constexpr long c_code_error_http_prepare         =  -3;
constexpr long c_code_error_http_prepare_no_body =  -4;
constexpr long c_code_error_http_prepare_body    =  -5;
constexpr long c_code_error_http_prepare_mime    =  -6;
constexpr long c_code_error_http_method          =  -7;
constexpr long c_code_error_http_mime            =  -8;
constexpr long c_code_error_wrapper_prepare      =  -9;
constexpr long c_code_error_authentication       = -10;

// The Wrapper class is used to handle all the common curl processing
// and communication with ASync.

// The base class is the one known and called by ASync
class WrapperBase
{
public:
  explicit WrapperBase() {};
  virtual ~WrapperBase() {};
  //
protected:
  friend class ASync;
  //
  // Called when a transfer is finished
  virtual void async_cb( long p_result ) = 0;
  //
  // Prevent copy
  WrapperBase( const WrapperBase & )             = delete;
  WrapperBase & operator=( const WrapperBase & ) = delete;
};

// Exception thrown when the factory fails to create a new protocol class
struct bad_curl_easy_alloc : public std::bad_alloc
{
  const char * what() const noexcept override { return "Initializing curl easy handle"; }
};

template< typename Protocol >
class Wrapper: public WrapperBase
{
  private:
    // Used to access the protected constructor of Protocol
    struct ProtocolPublic: public Protocol
    {
        explicit ProtocolPublic( ASync & p_async ): Protocol( p_async ) {}
    };
    //
  public:
    // The factory. The invoker takes the ownership of the new class, but
    // the class itself keeps a weak pointer to the return shared pointer.
    // It is used later to create extra shared pointers.
    static std::shared_ptr< Protocol > create( ASync & p_async )
    {
      auto wrapper = std::make_shared< ProtocolPublic >( p_async ); // throw on memory error;
      //
      wrapper->m_curl = p_async.get_handle(); // allocate and configure the curl easy handle
      if ( wrapper->m_curl == nullptr )
        throw bad_curl_easy_alloc();
      //
      wrapper->m_self_weak = wrapper;
      //
      return wrapper;
    }
    //
    explicit Wrapper( ASync & p_async ): WrapperBase(), m_async( p_async )
    { 
    }
    //
    virtual ~Wrapper()
    {
        m_async.return_handle( m_curl );
    }
    //
    // To start a transfer asynchronously.
    // To ensure persistency, a new shared pointer is created. This increases the
    // reference counter of the shared pointer, and thus the class continues
    // to exist even if the share pointer on by the user goes out of scope and is reset.
    Protocol & start( std::function<void( const Protocol & )> p_user_cb = nullptr )
    {
      if ( ! m_exec_running )
      {
        m_user_cb = p_user_cb; // will be cleared by cb_protocol
        //
        if ( prepare_protocol() && prepare_local() ) // set m_response_code on error
        {
          if ( auto self = m_self_weak.lock() ) // must succeed since we are invoked, shared must exist
          {
            if ( easy_setopt( m_curl, CURLOPT_PRIVATE, this ) ) // used (and cleared) by ASync to call async_cb()
            {
              m_exec_running = true;
              m_self_shared  = self; // count=3 (user shared, m_self_weak locked and m_self_shared)
              //
              m_async.start_request( m_curl ); // async processing starts here
              //
              return static_cast< Protocol & >( *this ); // count=2 (user shared and m_self_shared remains)
            }
          }
        }
        //
        // Failed case
        cb_protocol( static_cast< Protocol & >( *this ) ); // invoke user's callback, clear m_user_cb
      }
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Wait for the end of the asynchronous transfer (after start())
    Protocol & join( void )
    {
      {
        std::unique_lock lock( m_exec_mutex );
        if ( m_exec_running )
          m_exec_cv.wait( lock ); // wait for the end of the async processing
      }
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // To execute the transfer synchronously
    Protocol & exec( void )
    {
      return start().join(); // start and wait
    }
    //
    // Abort current request
    Protocol & abort( void )
    {
      m_async.abort_request( m_curl );
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Set easy libcurl options. Can be called several times
    Protocol & options( const std::string & p_options )
    {
      if ( m_response_code == c_code_success )
        if ( ! m_options.set( p_options ) )
          m_response_code = c_code_error_option;
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Set easy libcurl credential
    Protocol & authentication( const std::string & p_credential )
    {
      if ( m_response_code == c_code_success )
        if ( ! m_authentication.set( p_credential ) )
          m_response_code = c_code_error_authentication;
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Reset the protocol before starting a new transfer
    Protocol & clear( void )
    {
      if ( ! m_exec_running )
      {
        clear_protocol();
        //
        m_options.clear();
        m_authentication.clear();
        m_response_code = c_code_success;
      }
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Accessors
    long get_code( void ) const { return m_response_code; };
    //
  protected:
    CURL *         m_curl = nullptr;
    Options        m_options;
    Authentication m_authentication;
    long           m_response_code = c_code_success;
    //
  protected:
    //
    // When starting, applies the local configuration.
    // It is guaranteed that there is no operation running.
    bool prepare_local( void )
    {
      bool ok = true;
      //
      ok = ok && m_options.apply( m_curl );
      ok = ok && m_authentication.apply( m_curl );
      //
      if ( ok )
        return true;
      //
      m_response_code = c_code_error_wrapper_prepare;
      return false;
    }
    //
    // Called by ASync when the asynchronous transfer is finished
    void async_cb( long p_result ) override
    {
      m_response_code = p_result;
      //
      finalize_protocol(); // call Protocol to retrieve protocol related details
      //
      // First the notification (release the join()): transfer is finished
      {
        std::lock_guard lock( m_exec_mutex );
        m_exec_running = false;
        m_exec_cv.notify_one();
      }
      //
      cb_protocol( *m_self_shared ); // invoke user's callback, clear m_user_cb
      //
      // To avoid any reentrancy problem on self destruction (reset() calls destructor
      // that calls reset()...), use a local temporary variable
      {
        auto tmp = m_self_shared; // increment count, now 2 or 3 (if user shared still exist)
        m_self_shared.reset();    // now 1 or 2 (if user shared still exist)
        tmp.reset();              // now 0 (object destructed) or 1 (if user shared still exist)
      }
    }
    //
    // When starting, the Protocol configures the easy handle
    virtual bool prepare_protocol( void ) = 0;
    //
    // When the transfer is finished, the Protocol retrieves protocol related details
    virtual void finalize_protocol( void ) = 0;
    //
    // When doing a clear() to reset the Protocol options
    virtual void clear_protocol( void ) = 0;
    //
    // Invoke the Protocol user's callback
    void cb_protocol( const Protocol & p_protocol )
    {
      try
      {
        if ( m_user_cb != nullptr )
        {
          m_user_cb( p_protocol ); // pass as const: the user can't restart a transfer
          m_user_cb = nullptr;
        }
      }
      catch ( ... )
      {
      }
    }
    //
    // Return true is the request is executing, used to prevent modifications
    // of the persistent data used by libcurl.
    bool is_running( void ) const { return m_exec_running; }
    //
  private:
    ASync &                     m_async;
    std::weak_ptr< Protocol >   m_self_weak;   // set at creation
    std::shared_ptr< Protocol > m_self_shared; // set while the transfer is executing to ensure object lifetime
    //
    std::function< void( const Protocol & ) > m_user_cb = nullptr;
    //
    std::mutex              m_exec_mutex;
    std::condition_variable m_exec_cv;
    bool                    m_exec_running = false;
};

} // namespace curlev
