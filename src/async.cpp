/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <chrono>
#include <cstring>
#include <map>
#include <string>

#include "async.hpp"
#include "wrapper.hpp"
#include "utils/assert_return.hpp"
#include "utils/curl_utils.hpp"
#include "utils/string_utils.hpp"

#if LIBCURL_VERSION_NUM < CURL_VERSION_BITS( 7, 87, 0 )
#define CURL_WRITEFUNC_ERROR 0 // added in 7.87.0
#endif

namespace curlev
{

namespace
{
  // A safety in case a notification is lost
  constexpr auto c_event_wait_timeout = std::chrono::milliseconds( 1'000 );

  // Short sleep delay when doing active wait
  constexpr auto c_short_wait_ms      = 10U;

  // Some magic values: it is possible to start around 300req/ms in curl_multi_add_handle()
  constexpr auto c_requests_per_ms    = 300U;

  // Cleanly close and deallocate a loop
  void uv_clear_loop( uv_loop_t *& p_loop )
  {
    if ( p_loop != nullptr )
    {
      while ( uv_loop_close( p_loop ) == UV_EBUSY )
        uv_sleep( c_short_wait_ms );
      //
      delete p_loop;
      p_loop = nullptr;
    }
  }
} // namespace

//--------------------------------------------------------------------
ASync::ASync()
{
  m_uv_timer.data = nullptr;
}

//--------------------------------------------------------------------
ASync::~ASync()
{
  stop();
}

//--------------------------------------------------------------------
// Must be called at least once before doing calling any other function of curlev.
// Start curl, share and multi, then UV and its worker thread.
bool ASync::start()
{
  if ( m_uv_running )
    return true;
  //
  bool ok = true;
  //
  ok = ok && global_init();
  ok = ok && share_init();
  ok = ok && multi_init();
  ok = ok && uv_init(); // set m_uv_running to true
  ok = ok && cb_init(); // set m_cb_running to true
  //
  m_default_options       .set_default();
  m_default_authentication.set_default();
  m_default_certificates  .set_default( m_global_ca_info, m_global_ca_path );
  //
  return ok;
}

//--------------------------------------------------------------------
// Must be called at least when the program stops.
// Waits a maximum of p_timeout_ms milliseconds before forcefully stopping,
// returns true if stopping was forced.
// Stop the worker thread used by UV and CB, clear share and multi of curl.
// Ensure all UV handles are released.
// Clean curl.
bool ASync::stop( unsigned p_timeout_ms /* = 30000 */)
{
  bool forced = ! wait_pending_requests( p_timeout_ms );
  //
  uv_clear();
  cb_clear();
  share_clear();
  multi_clear();
  global_clear();
  //
  return forced;
}

//--------------------------------------------------------------------
// Setting default values for options, authentication and certificates
bool ASync::options( const std::string & p_options )
{
  std::unique_lock lock( m_default_locks );
  //
  return m_default_options.set( p_options );
}

bool ASync::authentication( const std::string & p_credential )
{
  std::unique_lock lock( m_default_locks );
  //
  return m_default_authentication.set( p_credential );
}

bool ASync::certificates( const std::string & p_certificates )
{
  std::unique_lock lock( m_default_locks );
  //
  return m_default_certificates.set( p_certificates );
}

//--------------------------------------------------------------------
// Retrieve the current default options and authentication
void ASync::get_default( Options & p_options, Authentication & p_authentication, Certificates & p_certificates ) const
{
  std::shared_lock lock( m_default_locks );
  //
  p_options        = m_default_options;
  p_authentication = m_default_authentication;
  p_certificates   = m_default_certificates;
}

//--------------------------------------------------------------------
// Create a new easy handle that *must* be freed using return_handle().
CURL * ASync::get_handle( WrapperBase * p_protocol ) const
{
  ASSERT_RETURN( p_protocol != nullptr, nullptr ); // bad protocol
  //
  auto * curl = curl_easy_init();
  bool   ok   = true;
  //
  ok = ok && curl != nullptr;
  ok = ok && easy_setopt( curl, CURLOPT_WRITEFUNCTION , curl_cb_write  );
  ok = ok && easy_setopt( curl, CURLOPT_WRITEDATA     , p_protocol     );
  ok = ok && easy_setopt( curl, CURLOPT_HEADERFUNCTION, curl_cb_header );
  ok = ok && easy_setopt( curl, CURLOPT_HEADERDATA    , p_protocol     );
  ok = ok && easy_setopt( curl, CURLOPT_SHARE         , m_share_handle );
  ok = ok && easy_setopt( curl, CURLOPT_NOSIGNAL      , 1L             );
  //
  p_protocol->m_retry_uv_timer.data = curl; // the timer used for retry points on the curl handle
  //
  if ( ! ok )
  {
    curl_easy_cleanup( curl ); // ok on nullptr
    return nullptr;
  }
  //
  return curl;
}

//--------------------------------------------------------------------
// Release a handle previously allocated by get_handle, ok on nullptr
void ASync::return_handle( CURL * & p_curl )
{
  curl_easy_cleanup( p_curl ); // ok on nullptr
  p_curl = nullptr;
}

//--------------------------------------------------------------------
// Starts the transfer, ok on nullptr.
// Waits the end of the current uv_run() to add the handle.
bool ASync::start_request( CURL * p_curl, void * p_protocol_cb )
{
  if ( ! easy_setopt( p_curl, CURLOPT_PRIVATE, p_protocol_cb ) )
    return false;
  //
  m_nb_running_requests++; // the running state includes the waiting period
  //
  m_nb_waiting_requests++;
  std::lock_guard lock( m_uv_run_mutex );
  m_nb_waiting_requests--;
  //
  // Added for the next uv_run() (in worker thread of uv_init())
  if ( curl_multi_add_handle( m_multi_handle, p_curl ) == CURLM_OK ) // ok on nullptr
  {
    m_uv_run_cv.notify_one();
    return true;
  }
  //
  m_nb_running_requests--;
  return false;
}

//--------------------------------------------------------------------
// Aborts a request previously started
void ASync::abort_request( CURL * p_curl )
{
  std::lock_guard lock( m_uv_run_mutex );
  //
  // Returns CURLM_OK if the easy handle is removed or was not present (already removed)
  auto result_code = curl_multi_remove_handle( m_multi_handle, p_curl ); // ok on nullptr
  //
  if ( result_code == CURLM_OK )
    request_completed( p_curl, CURLE_ABORTED_BY_CALLBACK ); // wrapper cannot be deleted here since it is currently calling us
}

//--------------------------------------------------------------------
// Number or curl_global_init() must match the number of curl_global_cleanup()

#if LIBCURL_VERSION_NUM >= CURL_VERSION_BITS( 7, 84, 0 )
namespace
{
  // Retrieve the default CA path and file.
  // Before libcurl 7.84.0 it was not possible to retrieve them.
  bool get_default_ca( std::string & p_info, std::string & p_path )
  {
    if ( CURL * curl = curl_easy_init(); curl != nullptr )
    {
      char * cainfo = nullptr;
      char * capath = nullptr;
      //
      // On Windows CURLINFO_CAPATH ( and possibly CURLINFO_CAINFO) may
      // not be defined: curl_easy_getinfo returns CURLE_OK, but path remains NULL.
      bool ok = ( curl_easy_getinfo( curl, CURLINFO_CAINFO, &cainfo ) == CURLE_OK ) &&
                ( curl_easy_getinfo( curl, CURLINFO_CAPATH, &capath ) == CURLE_OK );
      //
      if ( ok )
      {
        p_info = cainfo != nullptr ? cainfo : "";
        p_path = capath != nullptr ? capath : "";
      }
      //
      curl_easy_cleanup( curl );
      //
      return ok;
    }
    //
    return false;
  }
} // namespace
#endif

bool ASync::global_init()
{
  std::lock_guard lock( m_global_mutex );
  //
  bool ok = true;
  //
  if ( m_global_count == 0 ) // first initialization
  {
    ok = curl_global_init( CURL_GLOBAL_ALL ) == CURLE_OK;
    //
#if LIBCURL_VERSION_NUM >= CURL_VERSION_BITS( 7, 84, 0 )
    ok = ok && get_default_ca( m_global_ca_info, m_global_ca_path );
    //
    if ( ! ok )
      curl_global_cleanup();
#endif
  }
  //
  if ( ok )
    m_global_count++;
  //
  return ok;
}

//--------------------------------------------------------------------
// Number or curl_global_init() must match the number of curl_global_cleanup()
void ASync::global_clear()
{
  std::lock_guard lock( m_global_mutex );
  //
  if ( m_global_count > 0 )       // safety if global_clear() is invoked more than global_init()
    if ( --m_global_count == 0 )  // last one
      curl_global_cleanup();
}

//--------------------------------------------------------------------
// Cookies are not shared since the easy handles can be used in
// different contexts.
bool ASync::share_init()
{
  m_share_handle = curl_share_init();
  //
  bool ok = true;
  //
  ok = ok && m_share_handle != nullptr;
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_SHARE     , CURL_LOCK_DATA_CONNECT     );
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_SHARE     , CURL_LOCK_DATA_DNS         );
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_SHARE     , CURL_LOCK_DATA_SSL_SESSION );
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_LOCKFUNC  , &share_cb_lock             );
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_UNLOCKFUNC, &share_cb_unlock           );
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_USERDATA  , this                       );
  //
  if ( ! ok )
  {
    curl_share_cleanup( m_share_handle ); // ok on nullptr
    m_share_handle = nullptr;
  }
  //
  return ok;
}

//--------------------------------------------------------------------
void ASync::share_clear()
{
  curl_share_cleanup( m_share_handle ); // ok on nullptr
  m_share_handle = nullptr;
}

//--------------------------------------------------------------------
// Unlock some of curl share data
void ASync::share_cb_unlock(
    CURL *         /* easy */,
    curl_lock_data p_data,
    void *         p_user_ptr )
{
  auto * self = static_cast< ASync * >( p_user_ptr );
  //
  self->m_share_locks[ p_data ].unlock();
}

//--------------------------------------------------------------------
// Lock some of curl share data
void ASync::share_cb_lock(
    CURL *           /* easy */,
    curl_lock_data   p_data,
    curl_lock_access p_access,
    void *           p_user_ptr )
{
  auto * self = static_cast< ASync * >( p_user_ptr );
  //
  if ( p_access == CURL_LOCK_ACCESS_SHARED )
    self->m_share_locks[ p_data ].lock_shared();
  else
    self->m_share_locks[ p_data ].lock(); // CURL_LOCK_ACCESS_SINGLE and unknown
}

//--------------------------------------------------------------------
// Keep all the defaults maximum connection limits
bool ASync::multi_init()
{
  m_multi_handle = curl_multi_init();
  //
  bool ok = true;
  //
  ok = ok && m_multi_handle != nullptr;
  ok = ok && multi_setopt( m_multi_handle, CURLMOPT_SOCKETFUNCTION, &multi_cb_socket );
  ok = ok && multi_setopt( m_multi_handle, CURLMOPT_TIMERFUNCTION , &multi_cb_timer  );
  ok = ok && multi_setopt( m_multi_handle, CURLMOPT_SOCKETDATA    , this             );
  ok = ok && multi_setopt( m_multi_handle, CURLMOPT_TIMERDATA     , this             );
  //
  if ( ! ok )
  {
    curl_multi_cleanup( m_multi_handle ); // ok on nullptr
    m_multi_handle = nullptr;
  }
  //
  return ok;
}

//--------------------------------------------------------------------
void ASync::multi_clear()
{
  curl_multi_cleanup( m_multi_handle ); // ok on nullptr
  m_multi_handle = nullptr;
}

//--------------------------------------------------------------------
// Check if some multi operation are completed.
// Remove them and notify the wrapper.
// m_uv_run_mutex is locked.
namespace
{
  // Find the result to send to the wrapper: the libcurl error code if transfer failed,
  // or the protocol result code if transfer succeeded.
  long outcome_code( CURLMsg * p_message )
  {
    long code = CURL_LAST;
    //
    if ( p_message->data.result == CURLE_OK ) // transfer ok
    {
      auto protocol_code = curl_easy_getinfo( p_message->easy_handle, CURLINFO_RESPONSE_CODE, &code );
      //
      if ( protocol_code != CURLE_OK ) // failed to retrieve protocol code
        code = protocol_code;
    }
    else
    {
      code = p_message->data.result; // transfer failed
    }
    //
    return code;
  }
} // namespace

void ASync::multi_fetch_messages()
{
  CURLMsg * message = nullptr;
  int       pending = 0;
  //
  while ( ( message = curl_multi_info_read( m_multi_handle, &pending ) ) != nullptr )
  {
    switch ( message->msg )
    {
    case CURLMSG_DONE:
      curl_multi_remove_handle( m_multi_handle, message->easy_handle );
      //
      request_completed( message->easy_handle, outcome_code( message ) ); // wrapper can be deleted here, easy_handle may be invalid
      break;
    default:
      break;
    }
  }
}

//--------------------------------------------------------------------
// Called by curl_multi_socket_action(), curl_multi_add_handle()... to start and stop timers.
// m_uv_run_mutex is locked.
int ASync::multi_cb_timer(
    CURLM * /* multi */,
    long    p_timeout_ms,
    void *  p_clientp )
{
  bool   ok   = true;
  auto * self = static_cast< ASync * >( p_clientp );
  //
  ok = ok && self != nullptr; // always true
  //
  if ( p_timeout_ms < 0 ) // delete the timer
  {
    ok = ok && uv_timer_stop( &self->m_uv_timer ) == 0;
  }
  else // add or replace a timer
  {
    ok = ok && uv_timer_start( &self->m_uv_timer,
                               uv_timeout_cb,
                               static_cast< uint64_t >( p_timeout_ms ),
                               0 ) == 0;
  }
  //
  return ok ? 0 : -1; // on error all transfers in progress are aborted and fail
}

//--------------------------------------------------------------------
// Called by curl_multi_socket_action when there is an update on one of multi socket.
// Either we expect data (and call uv_poll_start) or are done (and call uv_poll_stop).
// m_uv_run_mutex is locked.
int ASync::multi_cb_socket(
    CURL *        /* easy */,
    curl_socket_t p_socket,
    int           p_what,
    void *        p_user_data,
    void *        p_socket_data )
{
  ASSERT_RETURN( p_user_data != nullptr, -1 ); // not possible
  //
  auto * self    = static_cast< ASync *       >( p_user_data   ); // CURLMOPT_SOCKETDATA
  auto * context = static_cast< CurlContext * >( p_socket_data ); // curl_multi_assign
  //
  bool ok     = true;
  int  events = 0;
  //
  switch ( p_what )
  {
  case CURL_POLL_IN:
  case CURL_POLL_OUT:
  case CURL_POLL_INOUT:
    if ( context == nullptr ) // the first time, create the context
      context = self->create_curl_context( p_socket );
    //
    if ( p_what != CURL_POLL_IN )
      events |= UV_WRITABLE;
    if ( p_what != CURL_POLL_OUT )
      events |= UV_READABLE;
    //
    ok = ok && context != nullptr;
    ok = ok && curl_multi_assign( self->m_multi_handle, p_socket, context ) == CURLM_OK;
    ok = ok && uv_poll_start( &context->poll, events, &uv_io_cb ) == 0;
    break;
  case CURL_POLL_REMOVE:
    ok = ok && context != nullptr;
    ok = ok && uv_poll_stop( &context->poll ) == 0;
    ok = ok && curl_multi_assign( self->m_multi_handle, p_socket, nullptr ) == CURLM_OK; // clear the context reference
    //
    destroy_curl_context( context ); // ok on nullptr
    break;
  default:
    break;
  }
  //
  return ok ? 0 : -1; // on error, all transfers currently in progress are aborted and fail
}

//--------------------------------------------------------------------
// Start UV loop and timer, and the worker thread waiting for IO
bool ASync::uv_init()
{
  bool ok = true;
  //
  ok = ok && m_uv_loop == nullptr; // already started
  ok = ok && ( m_uv_loop = new ( std::nothrow ) uv_loop_t ) != nullptr;
  ok = ok && 0 == uv_loop_init ( m_uv_loop );
  ok = ok && 0 == uv_timer_init( m_uv_loop, &m_uv_timer );
  //
  if ( ok )
  {
    m_uv_loop->data = this;
    m_uv_timer.data = this;
    m_uv_running    = true;
    m_uv_worker     = std::thread(
        [ this ]
        {
          std::unique_lock lock( m_uv_run_mutex );
          //
          while ( m_uv_running )
            if ( uv_run( m_uv_loop, UV_RUN_NOWAIT ) == 0 ) // if no requests are executing:
              uv_run_wait_requests( lock );                //   wait for new ones (unlock, wait, lock)
            else                                           // else
              uv_run_accept_requests( lock );              //   accept new ones (unlock, accept, lock)
        } );
  }
  else
  {
    uv_clear_loop( m_uv_loop ); // ok on nullptr
  }
  //
  return ok;
}

//--------------------------------------------------------------------
void ASync::uv_clear()
{
  m_uv_running = false;
  m_uv_run_cv.notify_one(); // speedup the exit of uv_run_wait_requests
  //
  if ( m_uv_worker.joinable() )
    m_uv_worker.join();
  //
  // m_uv_timer is set to this in start(): it is used to know if uv_timer_init was called
  if ( m_uv_timer.data != nullptr )
  {
    uv_timer_stop( &m_uv_timer ); // crashes if uv_timer_init was not called
    m_uv_timer.data = nullptr;
  }
  //
  if ( m_uv_loop != nullptr )
  {
    // see https://stackoverflow.com/questions/25615340/closing-libuv-handles-correctly
    //
    uv_stop( m_uv_loop );
    //
    uv_walk(
        m_uv_loop,
        []( uv_handle_t * handle, void * /* argument */ )
        {
          if ( uv_is_closing( handle ) == 0 )
            uv_close( handle, nullptr );
        },
        nullptr );
    //
    while ( uv_run( m_uv_loop, UV_RUN_ONCE ) != 0 )
      uv_sleep( c_short_wait_ms );
    //
    uv_clear_loop( m_uv_loop );
  }
}

//--------------------------------------------------------------------
// Called between uv_run by uv_init thread when there is requests executing.
// Try to sleep as little as possible, not at all if possible.
// unlock, accept start_request, lock
void ASync::uv_run_accept_requests( std::unique_lock< std::mutex > & p_lock ) const
{
  if ( m_nb_waiting_requests == 0 ) // then it is not even needed to unlock
    return;
  //
  p_lock.unlock();
  //
  uv_sleep( m_nb_waiting_requests / c_requests_per_ms );
  //
  p_lock.lock();
}

//--------------------------------------------------------------------
// Called between uv_run by uv_init thread when there is no request executing.
// unlock, wait for start_request, lock
void ASync::uv_run_wait_requests( std::unique_lock< std::mutex > & p_lock ) const
{
  m_uv_run_cv.wait_for( p_lock, c_event_wait_timeout );
}

//--------------------------------------------------------------------
void ASync::multi_update_running_stats( int p_running_handles )
{
  int previous = m_multi_running_max.load();
  //
  while ( p_running_handles > previous &&
          ! m_multi_running_max.compare_exchange_strong( previous, p_running_handles ) )
    ;
  //
  m_multi_running_current = p_running_handles;
}

//--------------------------------------------------------------------
// Called by uv_run() when there is some data sent/received, inform multi,
// then check if the multi operation are finished.
// m_uv_run_mutex is locked.
void ASync::uv_io_cb(
  uv_poll_t * p_handle,
  int         /* status */,
  int         p_events )
{
  ASSERT_RETURN_VOID( p_handle != nullptr && p_handle->data != nullptr ); // not possible
  //
  auto * context = static_cast< CurlContext * >( p_handle->data );
  //
  uv_timer_stop( &context->async.m_uv_timer );
  //
  int flags = 0;
  if ( ( p_events & UV_READABLE ) != 0 ) flags |= CURL_CSELECT_IN;
  if ( ( p_events & UV_WRITABLE ) != 0 ) flags |= CURL_CSELECT_OUT;
  //
  int running_handles = 0;
  curl_multi_socket_action( context->async.m_multi_handle, context->curl, flags, &running_handles ); // call multi_cb_socket
  //
  context->async.multi_update_running_stats( running_handles );
  //
  context->async.multi_fetch_messages(); // must be the last line as the wrapper can be deleted here
}

//--------------------------------------------------------------------
// Called by uv_run() when a timeout is triggered.
// The timer used is the one in ASync.
// m_uv_run_mutex is locked.
void ASync::uv_timeout_cb( uv_timer_t * p_handle )
{
  ASSERT_RETURN_VOID( p_handle != nullptr && p_handle->data != nullptr ); // not possible
  //
  auto * self = static_cast< ASync * >( p_handle->data );
  //
  // Inform multi that a timeout occurred
  int running_handles = 0;
  curl_multi_socket_action( self->m_multi_handle, CURL_SOCKET_TIMEOUT, 0, &running_handles ); // call multi_cb_socket
  //
  self->multi_update_running_stats( running_handles );
  //
  self->multi_fetch_messages(); // must be the last line as the wrapper can be deleted here
}

//--------------------------------------------------------------------
// Called by uv_run() when a request is programmed to restart by request_completed().
// The timer used is the one in WrapperBase.
// m_uv_run_mutex is locked.
void ASync::uv_restart_cb( uv_timer_t * p_handle ) // NOLINT( readability-function-cognitive-complexity )
{
  ASSERT_RETURN_VOID( p_handle != nullptr ); // not possible
  //
  uv_close(
      reinterpret_cast< uv_handle_t * >( p_handle ), // NOLINT( cppcoreguidelines-pro-type-reinterpret-cast )
      []( uv_handle_t * handle )
      {
        ASSERT_RETURN_VOID( handle             != nullptr &&
                            handle->data       != nullptr &&
                            handle->loop       != nullptr &&
                            handle->loop->data != nullptr ); // not possible
        //
        auto * curl = static_cast< CURL *  >( handle->data );
        auto * self = static_cast< ASync * >( handle->loop->data );
        //
        // Re-post the request
        if ( curl_multi_add_handle( self->m_multi_handle, curl ) == CURLM_OK ) // ok on nullptr
          return;
        //
        // On error, inform the Wrapper
        auto * wrapper = ASync::get_wrapper_from_curl( curl );
        //
        ASSERT_RETURN_VOID( wrapper != nullptr && *wrapper ); // not possible
        //
        self->post_to_wrapper( curl, wrapper, c_error_internal_restart );
      } );
}

//--------------------------------------------------------------------
// The context is shared between multi and UV:
//  - as multi socket data (curl_multi_assign)
//  - in UV poll member data (here)
// m_uv_run_mutex is locked.
ASync::CurlContext * ASync::create_curl_context( curl_socket_t p_socket )
{
  auto * context = new ( std::nothrow ) CurlContext( *this, p_socket );
  if ( context == nullptr )
    return nullptr;
  //
  if ( uv_poll_init_socket( m_uv_loop, &context->poll, p_socket ) == 0 )
  {
    context->poll.data = context;
  }
  else
  {
    delete context;
    context = nullptr;
  }
  //
  return context;
}

//--------------------------------------------------------------------
// Delete a context created by create_curl_context.
// Ok on nullptr.
// m_uv_run_mutex is locked.
void ASync::destroy_curl_context( CurlContext * p_context )
{
  if ( p_context != nullptr )
  {
    // From doc: uv_handle_t is the base type for all handles...any libuv handle can be cast to uv_handle_t
    uv_close(
        reinterpret_cast< uv_handle_t * >( &p_context->poll ), // NOLINT( cppcoreguidelines-pro-type-reinterpret-cast )
        []( uv_handle_t * handle )
        {
          ASSERT_RETURN_VOID( handle != nullptr && handle->data != nullptr ); // not possible
          //
          auto * context = static_cast< CurlContext * >( handle->data );
          delete context;
        } );
  }
}

//--------------------------------------------------------------------
// Start the CB worker thread that will call the Wrapper callback
bool ASync::cb_init()
{
  m_cb_running = true;
  m_cb_worker  = std::thread(
      [ this ]
      {
        std::unique_lock lock( m_cb_mutex );              // lock
        //
        while ( m_cb_running )
        {
          while ( ! m_cb_queue.empty() )                  // if some notifications are pending
          {
            auto [ wrapper, p_result_code ] = m_cb_queue.front();
            m_cb_queue.pop_front();
            lock.unlock();                                // retrieve the notification, unlock
            //
            invoke_wrapper( wrapper, p_result_code );
            //
            lock.lock();                                  // lock
          }
          //
          m_cb_cv.wait_for( lock, c_event_wait_timeout ); // unlock, wait, lock
        }
      } );
  //
  return true;
}

//--------------------------------------------------------------------
// Stop the CB worker thread
void ASync::cb_clear()
{
  m_cb_running = false;
  m_cb_cv.notify_one(); // speedup the exit of cb_init's thread
  //
  if ( m_cb_worker.joinable() )
    m_cb_worker.join();
}

//--------------------------------------------------------------------
// To store data received during the transfer.
// Data is located in the Protocol and is a std::string.
// m_uv_run_mutex is locked.
size_t ASync::curl_cb_write( const char * p_ptr, size_t p_size, size_t p_nmemb, void * p_userdata )
{
  if ( p_userdata == nullptr )
    return CURL_WRITEFUNC_ERROR;
  //
  auto *     protocol = static_cast< WrapperBase * >( p_userdata );
  const auto to_add   = p_size * p_nmemb;
  //
  // Reserve buffer if content-length is known and not yet reserved
  if ( protocol->m_header_content_length > 0 )
  {
    // Prevent reserving more than allowed
    if ( protocol->m_header_content_length > protocol->get_max_response_size() )
      return CURL_WRITEFUNC_ERROR;
    //
    protocol->m_response_body.reserve( protocol->m_header_content_length );
    protocol->m_header_content_length = 0;
  }
  //
  // Prevent exceeding max response size
  if ( protocol->m_response_body.size() + to_add > protocol->get_max_response_size() )
    return CURL_WRITEFUNC_ERROR;
  //
  try
  {
    protocol->m_response_body.append( p_ptr, to_add );
  }
  catch ( ... )
  {
    return CURL_WRITEFUNC_ERROR;
  }
  //
  return to_add;
}

//--------------------------------------------------------------------
// To store header received during the transfer.
// Data is located in the Protocol and is a std::map< std::string, std::string >.
// m_uv_run_mutex is locked.
size_t ASync::curl_cb_header( const char * p_buffer, size_t p_size, size_t p_nitems, void * p_userdata )
{
  static const std::string c_content_length = "content-length";
  //
  if ( p_userdata == nullptr )
    return CURL_WRITEFUNC_ERROR;
  //
  auto * protocol = static_cast< WrapperBase * >( p_userdata );
  auto   line     = std::string_view( p_buffer, p_size * p_nitems );
  auto   colon    = line.find( ':' );
  //
  if ( colon != std::string_view::npos )
  {
    auto key   = std::string( trim( line.substr( 0, colon  ) ) ); // key
    auto value =              trim( line.substr( colon + 1 ) )  ; // value
    //
    if ( ! key.empty() )
    {
      // Capture the body length: it will be used later in curl_cb_write
      if ( equal_ascii_ci( key, c_content_length ) )
        if ( ! svtoul( value, protocol->m_header_content_length ) )
          return CURL_WRITEFUNC_ERROR;
      //
      protocol->m_response_headers.insert_or_assign( std::move( key ), value );
    }
  }
  //
  return p_nitems * p_size;
}

//--------------------------------------------------------------------
// Wait for all pending requests to finish.
// Wait the maximum given tame, returns false if timeout is reached
bool ASync::wait_pending_requests( unsigned p_timeout_ms ) const
{
  for ( unsigned attempts = p_timeout_ms / c_short_wait_ms; attempts > 0; attempts-- )
  {
    if ( m_nb_running_requests == 0 )
      return true;
    //
    uv_sleep( c_short_wait_ms );
  }
  //
  return false;
}

//--------------------------------------------------------------------
// Handles the termination of the request. If configured, the request
// may be restarted.
// m_uv_run_mutex is locked.
namespace
{
  // Check if the code guarantees that the request can be resubmitted without side effect
  bool safe_to_restart_outcome( long p_result_code )
  {
    return p_result_code == CURLE_COULDNT_RESOLVE_HOST
        || p_result_code == CURLE_COULDNT_RESOLVE_PROXY
        || p_result_code == CURLE_COULDNT_CONNECT
        || p_result_code == CURLE_SSL_CONNECT_ERROR
        || p_result_code == CURLE_PEER_FAILED_VERIFICATION
        || p_result_code == 429 // Too Many Requests    NOLINT( cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers )
        || p_result_code == 503 // Service Unavailable  NOLINT( cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers )
        ;
  }
} // namespace

void ASync::request_completed( CURL * p_curl, long p_result_code )
{
  auto * wrapper = get_wrapper_from_curl( p_curl );
  if ( wrapper == nullptr || ! *wrapper )
    return;
  //
  if ( ( *wrapper )->can_reattempt() && safe_to_restart_outcome( p_result_code ) )
  {
    bool ok = true;
    //
    ok = ok && 0 == uv_timer_init ( m_uv_loop, &( *wrapper )->m_retry_uv_timer );
    ok = ok && 0 == uv_timer_start( &( *wrapper )->m_retry_uv_timer, uv_restart_cb,
                                    ( *wrapper )->get_retry_delay_ms(), 0 );
    //
    if ( ok )
      return;
    //
    // Keep the original result code if the restart fails
  }
  //
  post_to_wrapper( p_curl, wrapper, p_result_code );
}

//--------------------------------------------------------------------
// Returns the operation outcome to the wrapper, depending on the config:
//  1] push the job in the queue, callback thread will call the Wrapper.
//  2] call the Wrapper from UV worker thread.
// m_uv_run_mutex is locked.
void ASync::post_to_wrapper(
    CURL *                     p_curl,
    t_wrapper_shared_ptr_ptr & p_wrapper,
    long                       p_result_code )
{
  ASSERT_RETURN_VOID( p_curl != nullptr ); // not possible
  //
  curl_easy_setopt( p_curl, CURLOPT_PRIVATE, nullptr ); // set when Wrapper start(), cleared here
  //
  ASSERT_RETURN_VOID( p_wrapper != nullptr && *p_wrapper ); // not possible
  //
  if ( ( *p_wrapper )->use_threaded_cb() ) // push it to CB queue for later delivery
  {
    std::lock_guard lock( m_cb_mutex );
    //
    m_cb_queue.emplace_back( p_wrapper, p_result_code );
    m_cb_cv.notify_one();
  }
  else // call it now and here (in uv_run worker thread)
  {
    invoke_wrapper( p_wrapper, p_result_code );
  }
}

//--------------------------------------------------------------------
// Call the wrapper, delete the shared_ptr.
// m_uv_run_mutex may be locked.
void ASync::invoke_wrapper( t_wrapper_shared_ptr_ptr & p_wrapper, long p_result_code )
{
  ASSERT_RETURN_VOID( p_wrapper != nullptr && *p_wrapper );
  //
  try
  {
    ( *p_wrapper )->async_cb( p_result_code ); // call Protocol
  }
  catch ( ... )
  {
    m_protocol_has_crashed = true;
  }
  //
  m_nb_running_requests--;
  //
  ( *p_wrapper ).reset(); // possibly delete Protocol
  delete p_wrapper;
  p_wrapper = nullptr;
}

//--------------------------------------------------------------------
// Retrieve the Wrapper from the curl handle
// It is set when Wrapper start(), and cleared in post_to_wrapper()
ASync::t_wrapper_shared_ptr_ptr ASync::get_wrapper_from_curl( CURL * p_curl )
{
  void * cb_data = nullptr;
  if ( curl_easy_getinfo( p_curl, CURLINFO_PRIVATE, &cb_data ) != CURLE_OK ||
       cb_data == nullptr ) // already notified
    return nullptr;
  //
  return static_cast< ASync::t_wrapper_shared_ptr_ptr >( cb_data ); // allocated by Wrapper start()
}

} // namespace curlev
