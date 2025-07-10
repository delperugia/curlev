/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <cassert>
#include <condition_variable>
#include <curl/curl.h>
#include <functional>
#include <memory>
#include <mutex>

#include "async.hpp"
#include "common.hpp"

namespace curlev
{

constexpr long c_success                          =   0;
constexpr long c_running                          =  -1; // request is still running
constexpr long c_error_authentication_format      =  -2; // bad authentication format string
constexpr long c_error_authentication_set         =  -3; // bad authentication value
constexpr long c_error_certificates_format        =  -4; // bad certificates format string
constexpr long c_error_certificates_set           =  -5; // bad certificates value
constexpr long c_error_options_format             =  -6; // bad options format string
constexpr long c_error_options_set                =  -7; // bad option value
constexpr long c_error_user_callback              =  -8; // callback crashed
constexpr long c_error_internal_protocol_crashed  =  -9; // protocol crashed whiled invoked by ASync
constexpr long c_error_internal_start             = -10; // internal error
constexpr long c_error_http_headers_set           = -11; // bad header
constexpr long c_error_http_method_set            = -12; // internal error
constexpr long c_error_http_mime_set              = -13; // bad MIME value

// The default maximal received response size
constexpr auto c_default_max_response_size        = 2'000'000;

//--------------------------------------------------------------------
// The base class is the one known and called by ASync
class WrapperBase
{
public:
  virtual ~WrapperBase( void ) = default;
  //
protected:
  friend class ASync;
  //
  // Called when a transfer is finished
  virtual void async_cb( long p_result ) = 0;
  //
  // Is async_cb called in ASync uv thread (false) or a dedicated thread (true)
  virtual bool use_threaded_cb( void ) const = 0;
  //
  // Maximum size of the body that ASync will receive
  virtual size_t get_max_response_size( void ) const = 0;
  //
  // Data received during transfer by ASync's callbacks
  t_key_values_ci m_response_headers;          // must be persistent (CURLOPT_HEADERDATA)
  std::string     m_response_body;             // must be persistent (CURLOPT_WRITEDATA)
  size_t          m_header_content_length = 0; // set to the received Content-Length header, if received; reset when receiving body
};

//--------------------------------------------------------------------
// Exception thrown when the factory fails to create a new protocol class
struct bad_curl_easy_alloc : public std::bad_alloc
{
  // cppcheck-suppress unusedFunction
  [[nodiscard]] const char * what( void ) const noexcept override
  {
    return "Initializing curl easy handle";
  }
};

//--------------------------------------------------------------------
// The Wrapper class is used to handle all the common curl processing
// and communication with ASync.
template< typename Protocol >
class Wrapper: public WrapperBase
{
  private:
    // Used to access the protected constructor of Protocol
    struct ProtocolPublic : public Protocol
    {
      explicit ProtocolPublic( ASync & p_async ) : Protocol( p_async ) {}
    };
    //
  public:
    explicit Wrapper( ASync & p_async ) :
      WrapperBase(),
      m_async( p_async )
    {}
    //
    ~Wrapper( void ) override
    {
      ASync::return_handle( m_curl );
    }
    //
    // The factory. The invoker takes the ownership of the new class, but
    // the class itself keeps a weak pointer to the return shared pointer.
    // It is used later to create extra shared pointers.
    static std::shared_ptr< Protocol > create( ASync & p_async )
    {
      auto wrapper = std::make_shared< ProtocolPublic >( p_async ); // throw on memory error;
      //
      wrapper->m_curl = p_async.get_handle( wrapper.get() ); // allocate and configure the curl easy handle
      if ( wrapper->m_curl == nullptr )
        throw bad_curl_easy_alloc();
      //
      wrapper->m_self_weak = wrapper; // used to create and pass a shared_ptr to ASync
      //
      return wrapper;
    }
    //
    // To start a transfer asynchronously.
    // To ensure persistency, a new shared pointer is created and passed to ASync.
    // This increases the reference counter of the shared pointer, and thus the class continues
    // to exist even if the share pointer owned by the user goes out of scope and is reset.
    Protocol & start( std::function< void( const Protocol & ) > p_user_cb = nullptr )
    {
      {
        std::lock_guard lock( m_exec_mutex );
        //
        if ( m_exec_state != State::idle )              // already running: do nothing at all
          return static_cast< Protocol & >( *this );
        //
        m_user_cb = p_user_cb;                          // will be cleared by cb_protocol (below or in async_cb)
        //
        if ( m_response_code == c_success )             // initialization succeeded
        {
          if ( prepare_protocol() && prepare_local() )  // set m_response_code on error
          {
            if ( auto self = m_self_weak.lock() )       // must succeed since we are invoked
            {
              auto cb_data = new std::shared_ptr< Protocol >( self ); // a new shared_ptr for ASync, deleted by ASync
              m_exec_state = State::running;                          // will be cleared in async_cb called by ASync
              //
              if ( m_async.start_request( m_curl, cb_data ) )         // ASync processing starts here
                  return static_cast< Protocol & >( *this );
              //
              m_exec_state    = State::idle;                          // ASync failed
              m_response_code = c_error_internal_start;
              delete cb_data;
            }
          }
        }
      }
      //
      cb_protocol(); // invoke user's callback (outside of the lock), clear m_user_cb
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Wait for the end of the asynchronous transfer (after start())
    Protocol & join( void )
    {
      {
        std::unique_lock lock( m_exec_mutex );
        //
        if ( m_exec_state != State::idle )
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
      if ( is_running() )
        m_async.abort_request( m_curl );
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Set easy libcurl options. Can be called several times
    Protocol & options( const std::string & p_options )
    {
      do_if_idle( [ & ]() {
        if ( m_response_code == c_success )
          if ( ! m_options.set( p_options ) )
            m_response_code = c_error_options_format;
      } );
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Set easy libcurl credential
    Protocol & authentication( const std::string & p_credential )
    {
      do_if_idle( [ & ]() {
        if ( m_response_code == c_success )
          if ( ! m_authentication.set( p_credential ) )
            m_response_code = c_error_authentication_format;
      } );
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Set easy libcurl certificates
    Protocol & certificates( const std::string & p_certificates )
    {
      do_if_idle( [ & ]() {
        if ( m_response_code == c_success )
          if ( ! m_certificates.set( p_certificates ) )
            m_response_code = c_error_certificates_format;
      } );
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Set the callback mode (default true: threaded)
    Protocol & threaded_callback( bool p_mode )
    {
      do_if_idle( [ & ]() {
        if ( m_response_code == c_success )
          m_user_cb_threaded = p_mode;
      } );
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Set the maximal response size (default 2MB)
    Protocol & maximal_response_size( size_t p_size )
    {
      do_if_idle( [ & ]() {
        if ( m_response_code == c_success )
          m_max_response_size = p_size;
      } );
      //
      return static_cast< Protocol & >( *this );
    }
    //
    // Accessors
    long get_code( void ) const noexcept { return is_running() ? c_running : m_response_code; };
    //
    // Prevent copy
    Wrapper            ( const Wrapper & ) = delete;
    Wrapper & operator=( const Wrapper & ) = delete;
    Wrapper            ( Wrapper &&      ) = delete;
    Wrapper & operator=( Wrapper &&      ) = delete;
    //
  protected:
    CURL *         m_curl          = nullptr;
    long           m_response_code = c_success;
    Options        m_options;
    Authentication m_authentication;
    Certificates   m_certificates;
    //
    // Reset the protocol before starting a new transfer.
    // m_exec_state must be idle and m_exec_mutex locked (use do_if_idle).
    void clear( void )
    {
      assert( ! m_exec_mutex.try_lock() );
      //
      m_response_code         = c_success;
      m_header_content_length = 0;
      m_user_cb_threaded      = true;
      m_user_cb               = nullptr;
      m_max_response_size     = c_default_max_response_size;
      //
      m_async.get_default( m_options, m_authentication, m_certificates ); // restore global defaults
      //
      clear_protocol();
    }
    //
    // When starting, applies the local configuration.
    // It is guaranteed that there is no operation running.
    bool prepare_local( void )
    {
      if ( ! m_options.apply( m_curl ) )
      {
        m_response_code = c_error_options_set;
        return false;
      }
      //
      if ( ! m_authentication.apply( m_curl ) )
      {
        m_response_code = c_error_authentication_set;
        return false;
      }
      //
      if ( ! m_certificates.apply( m_curl ) )
      {
        m_response_code = c_error_certificates_set;
        return false;
      }
      //
      return true;
    }
    //
    // Called by ASync when the asynchronous transfer is finished
    void async_cb( long p_result ) override
    {
      m_response_code = p_result; // before finalize, in case the Protocol needs it
      //
      finalize_protocol(); // calls Protocol to retrieve protocol related details
      //
      {
        std::lock_guard lock( m_exec_mutex );
        m_exec_state = State::finished; // transfer is now finished, received data can be read
      }
      //
      cb_protocol(); // invokes user's callback, clear m_user_cb
      //
      {
        std::lock_guard lock( m_exec_mutex );
        m_exec_state = State::idle;
        m_exec_cv.notify_one(); // releases the join(): request is now terminated
      }
    }
    //
    // Is async_cb called in ASync uv thread (false) or a dedicated thread (true)
    // If there is no user CB (only our async_cb code), consider that it is fast
    // enough and don't use an extra thread.
    bool use_threaded_cb( void ) const override
    {
      return m_user_cb_threaded && m_user_cb != nullptr;
    }
    //
    // Retrieve the maximum size of the body that ASync will receive
    size_t get_max_response_size( void ) const override
    {
      return m_max_response_size;
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
    // Invoke the Protocol user's callback, clear m_user_cb
    void cb_protocol( void )
    {
      try
      {
        if ( m_user_cb != nullptr )
        {
          m_user_cb( static_cast< const Protocol & >( *this ) ); // pass as const: the user can't restart a transfer
          m_user_cb = nullptr;
        }
      }
      catch ( ... )
      {
        m_response_code = c_error_user_callback; // this overwrite the real result
      }
    }
    //
    // Execute an action if a request is just created or terminated.
    // Returns true if is was executed
    template< typename action >
    bool do_if_idle( action p_action ) const
    {
      std::lock_guard lock( m_exec_mutex );
      //
      if ( m_exec_state == State::idle )
      {
        p_action();
        return true;
      }
      //
      return false;
    }
    //
    // Transfer is active (or was since the lock goes out of scope)
    bool is_running( void ) const
    {
      std::lock_guard lock( m_exec_mutex );
      //
      return m_exec_state == State::running;
    }
    //
  private:
    ASync & m_async;
    //
    enum class State
    {
      idle,     // just created or terminated
      running,  // actively running
      finished  // transfer is finished but post processing is still taking place
    };
    //
    mutable std::mutex      m_exec_mutex;
    std::condition_variable m_exec_cv; // used by join()
    State                   m_exec_state = State::idle;
    //
    std::weak_ptr< Protocol > m_self_weak; // set on self at creation, used to create and pass a shared_ptr to ASync
    //
    // Optional user CB invoked from async_cb
    bool                                      m_user_cb_threaded  = true;
    std::function< void( const Protocol & ) > m_user_cb           = nullptr;
    size_t                                    m_max_response_size = c_default_max_response_size;
};

} // namespace curlev
