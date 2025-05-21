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

// The base class is the one known and called by ASync.
class WrapperBase
{
public:
  virtual ~WrapperBase() {};
  //
protected:
  friend class ASync;
  //
  // Called when a transfer is finished
  virtual void cb_back( long p_result ) = 0;
};

// Exception thrown when the factory fails to create a new protocol class.
struct bad_curl_easy_alloc : public std::bad_alloc
{
  const char * what() const noexcept override { return "Initializing curl easy handle"; }
};

template< typename Protocol >
class Wrapper: public WrapperBase
{
  private:
    // Used to access the protected constructor of Protocol.
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
    explicit Wrapper( ASync & p_async ): m_async( p_async )
    { 
    }
    //
    virtual ~Wrapper()
    {
        m_async.return_handle( m_curl );
    }
    //
    // When starting, applies the local configuration.
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
    // To start a transfer asynchronously.
    // To ensure persistency, a new shared pointer is created. This increases the
    // reference counter of the shared pointer, and thus the class continues
    // to exist even if the share pointer on by the user goes out of scope and is reset.
    Protocol & start( std::function<void( const Protocol & )> p_user_cb )
    {
      m_exec_done = false;
      //
      if ( ! prepare_protocol() || ! prepare_local() ) // set m_response_code on error
        return *dynamic_cast< Protocol * >( this );
      //
      if ( m_response_code == c_code_success ) // no error found while building the request
      {
        if ( auto self = m_self_weak.lock() ) // must succeed since we are invoked, shared must exist
        {
          if ( easy_setopt( m_curl, CURLOPT_PRIVATE, this ) ) // used by ASync to call back()
          {
            m_user_cb     = p_user_cb;
            m_self_shared = self; // count=3 (user shared, m_self_weak locked and m_self_shared)
            //
            m_async.start_request( m_curl ); // async processing starts here
            //
            return *dynamic_cast< Protocol * >( this ); // count=2 (user shared and m_self_shared remains)
          }
        }
      }
      //
      // Failed case.
      //
      if ( p_user_cb )
        p_user_cb( *dynamic_cast< Protocol * >( this ) );
      //
      {
        std::lock_guard lock( m_exec_mutex );
        m_exec_done = true;
        m_exec_cv.notify_one(); // to unlock the join the user will do
      }
      //
      return *dynamic_cast< Protocol * >( this );
    }
    //
    // Wait for the end of the asynchronous transfer (after start()).
    Protocol & join( void )
    {
      {
        std::unique_lock lock( m_exec_mutex );
        if ( ! m_exec_done )
          m_exec_cv.wait( lock ); // wait for the end of the async processing
      }
      //
      return *dynamic_cast< Protocol * >( this );
    }
    //
    // To execute the transfer synchronously.
    Protocol & exec( void )
    {
      return start( nullptr ).join(); // start and wait
    }
    //
    // Set easy libcurl options. Can be called several times.
    Protocol & options( const std::string & p_options )
    {
      if ( m_response_code == c_code_success )
        if ( ! m_options.set( p_options ) )
          m_response_code = c_code_error_option;
      //
      return *dynamic_cast< Protocol * >( this );
    }
    //
    // Set easy libcurl credential.
    Protocol & authentication( const std::string & p_credential )
    {
      if ( m_response_code == c_code_success )
        if ( ! m_authentication.set( p_credential ) )
          m_response_code = c_code_error_authentication;
      //
      return *dynamic_cast< Protocol * >( this );
    }
    //
    // Reset the protocol before starting a new transfer.
    virtual Protocol & clear( void )
    {
      m_options.clear();
      m_authentication.clear();
      m_response_code = c_code_success;
      //
      return *dynamic_cast< Protocol * >( this );
    }
    //
    // Accessors.
    long get_code( void ) const { return m_response_code; };
    //
  protected:
    CURL *         m_curl = nullptr;
    Options        m_options;
    Authentication m_authentication;
    long           m_response_code = c_code_success;
    //
    // Called by ASync when the asynchronous transfer is finished.
    void cb_back( long p_result ) override
    {
      m_response_code = p_result;
      //
      finalize_protocol(); // call Protocol to retrieve protocol related details
      //
      if ( m_user_cb )
        m_user_cb( *m_self_shared );
      //
      {
        std::lock_guard lock( m_exec_mutex );
        m_exec_done = true;
        m_exec_cv.notify_one();
      }
      //
      easy_setopt( m_curl, CURLOPT_PRIVATE, nullptr ); // set in start(), cleared here
      m_user_cb = nullptr;
      //
      // To avoid any reentrancy problem on self destruction (reset() calls destructor
      // that calls reset()...), use a local temporary variable.
      {
        auto tmp = m_self_shared; // increment count, now 2 or 3 (if user shared still exist)
        m_self_shared.reset();    // now 1 or 2 (if user shared still exist)
        tmp.reset();              // now 0 (object destructed) or 1 (if user shared still exist)
      }
    }
    //
    // When starting, the Protocol configures the easy handle.
    virtual bool prepare_protocol ( void )  = 0;
    //
    // When the transfer is finished, the Protocol retrieves protocol related details.
    virtual void finalize_protocol( void ) = 0;
    //
  private:
    ASync &                     m_async;
    std::weak_ptr< Protocol >   m_self_weak;   // set at creation
    std::shared_ptr< Protocol > m_self_shared; // set while the transfer is executing to ensure object lifetime
    //
    std::function< void( const Protocol & ) > m_user_cb = nullptr;
    //
    mutable std::mutex              m_exec_mutex;
    mutable std::condition_variable m_exec_cv;
    mutable bool                    m_exec_done = false;
};
