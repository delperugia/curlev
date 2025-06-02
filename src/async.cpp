/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <cstring>
#include <map>
#include <string>

#include "async.hpp"
#include "common.hpp"
#include "wrapper.hpp"

#if LIBCURL_VERSION_NUM < CURL_VERSION_BITS( 7, 87, 0 )
#define CURL_WRITEFUNC_ERROR 0 // added in 7.87.0
#endif

namespace curlev
{

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
  ok = ok && CURLE_OK == curl_global_init( CURL_GLOBAL_ALL );
  ok = ok && share_init();
  ok = ok && multi_init();
  ok = ok && uv_init(); // set m_uv_running to true
  ok = ok && cb_init(); // set m_cb_running to true
  //
  return ok;
}

//--------------------------------------------------------------------
// Must be called at least when the program stops.
// Waits a maximum of p_timeout_s seconds before forcefully stopping,
// returns true if stopping was forced.
// Stop the worker thread used by UV and CB, clear share and multi of curl.
// Ensure all UV handles are released.
// Clean curl.
bool ASync::stop( unsigned p_timeout_s /* = 30 */)
{
  bool forced = ! wait_pending_requests( p_timeout_s );
  //
  uv_clear();
  cb_clear();
  share_clear();
  multi_clear();
  //
  curl_global_cleanup();
  //
  return forced;
}

//--------------------------------------------------------------------
// Create a new easy handle that *must* be freed using return_handle.
// CURLOPT_WRITEDATA is properly set in the wrapper to one of its member.
CURL * ASync::get_handle() const
{
  auto curl = curl_easy_init();
  bool ok   = true;
  //
  ok = ok && curl != nullptr;
  ok = ok && easy_setopt( curl, CURLOPT_WRITEFUNCTION , curl_cb_write  );
  ok = ok && easy_setopt( curl, CURLOPT_WRITEDATA     , nullptr        );
  ok = ok && easy_setopt( curl, CURLOPT_HEADERFUNCTION, curl_cb_header );
  ok = ok && easy_setopt( curl, CURLOPT_HEADERDATA    , nullptr        );
  ok = ok && easy_setopt( curl, CURLOPT_SHARE         , m_share_handle );
  ok = ok && easy_setopt( curl, CURLOPT_NOSIGNAL      , 1L             );
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
// Previously retrieved by get_handle, ok on nullptr
void ASync::return_handle( CURL * p_curl ) const
{
  curl_easy_cleanup( p_curl ); // ok on nullptr
}

//--------------------------------------------------------------------
// Starts the transfer, ok on nullptr.
// If it fails the wrapper is notified with the curl result code.
void ASync::start_request( CURL * p_curl )
{
  CURLMcode result_code;
  //
  {
    m_nb_running_requests++;
    //
    std::lock_guard lock( m_uv_run_mutex );
    //
    // It will be added at the end of the current uv_run (worker thread of uv_init())
    result_code = curl_multi_add_handle( m_multi_handle, p_curl ); // ok on nullptr
    //
    if ( result_code == CURLM_OK )
    {
      m_uv_run_cv.notify_one();
    }
    else
    {
      notify_wrapper( p_curl, result_code ); // wrapper cannot be deleted here since it is currently calling us
    }
  }
}

//--------------------------------------------------------------------
// Aborts a request previously started
void ASync::abort_request( CURL * p_curl )
{
  CURLMcode result_code;
  //
  {
    std::lock_guard lock( m_uv_run_mutex );
    //
    // Returns CURLM_OK if the easy handle is removed or was not present (already removed)
    result_code = curl_multi_remove_handle( m_multi_handle, p_curl ); // ok on nullptr
    //
    if ( result_code == CURLM_OK )
      notify_wrapper( p_curl, CURLE_ABORTED_BY_CALLBACK ); // wrapper cannot be deleted here since it is currently calling us
  }
}

//--------------------------------------------------------------------
// Cookies are not shared since the easy handles can be used in
// different contexts.
bool ASync::share_init( void )
{
  m_share_handle = curl_share_init();
  //
  bool ok = true;
  //
  ok = ok && m_share_handle != nullptr;
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_SHARE     , CURL_LOCK_DATA_DNS );
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_SHARE     , CURL_LOCK_DATA_SSL_SESSION );
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_SHARE     , CURL_LOCK_DATA_CONNECT );
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_LOCKFUNC  , &share_cb_lock );
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_UNLOCKFUNC, &share_cb_unlock );
  ok = ok && share_setopt( m_share_handle, CURLSHOPT_USERDATA  , this );
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
void ASync::share_clear( void )
{
  curl_share_cleanup( m_share_handle ); // ok on nullptr
  m_share_handle = nullptr;
}

//--------------------------------------------------------------------
// Unlock some of curl share data
void ASync::share_cb_unlock(
    [[maybe_unused]] CURL * p_handle,
    curl_lock_data          p_data,
    void *                  p_user_ptr )
{
  auto self = static_cast< ASync * >( p_user_ptr );
  //
  self->m_share_locks[ p_data ].unlock();
}

//--------------------------------------------------------------------
// Lock some of curl share data
void ASync::share_cb_lock(
    [[maybe_unused]] CURL *           p_handle,
    curl_lock_data                    p_data,
    [[maybe_unused]] curl_lock_access p_access,
    void *                            p_user_ptr )
{
  auto self = static_cast< ASync * >( p_user_ptr );
  //
  self->m_share_locks[ p_data ].lock();
}

//--------------------------------------------------------------------
// Keep all the defaults maximum connection limits
bool ASync::multi_init( void )
{
  m_multi_handle = curl_multi_init();
  //
  bool ok = true;
  //
  ok = ok && m_multi_handle != nullptr;
  ok = ok && multi_setopt( m_multi_handle, CURLMOPT_SOCKETFUNCTION, &multi_cb_socket );
  ok = ok && multi_setopt( m_multi_handle, CURLMOPT_TIMERFUNCTION , &multi_cb_timer );
  ok = ok && multi_setopt( m_multi_handle, CURLMOPT_SOCKETDATA    , this );
  ok = ok && multi_setopt( m_multi_handle, CURLMOPT_TIMERDATA     , this );
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
void ASync::multi_clear( void )
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
    long code;
    //
    if ( p_message->data.result == CURLE_OK ) // transfer ok
    {
      auto protocol_code = curl_easy_getinfo( p_message->easy_handle, CURLINFO_RESPONSE_CODE, &code );
      //
      if ( protocol_code != CURLE_OK ) // failed to retrieve protocol code
        code = protocol_code;
    }
    else
      code = p_message->data.result; // transfer failed
    //
    return code;
  }
} // namespace

void ASync::multi_fetch_messages( void )
{
  CURLMsg * message;
  int       pending;
  //
  while ( ( message = curl_multi_info_read( m_multi_handle, &pending ) ) )
  {
    switch ( message->msg )
    {
    case CURLMSG_DONE:
      curl_multi_remove_handle( m_multi_handle, message->easy_handle );
      //
      notify_wrapper( message->easy_handle, outcome_code( message ) ); // wrapper can be deleted here, easy_handle may be invalid
      break;
    default:
      break;
    }
  }
}

//--------------------------------------------------------------------
// Called by curl_multi_socket_action(), curl_multi_add_handle()... to start and stop timers.
// m_uv_run_mutex is locked.
int ASync::multi_cb_timer( [[maybe_unused]] CURLM * p_multi, long p_timeout_ms, void * p_clientp )
{
  auto self = static_cast< ASync * >( p_clientp );
  bool ok   = true;
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
    [[maybe_unused]] CURL * p_easy,
    curl_socket_t           p_socket,
    int                     p_what,
    void *                  p_user_data,
    void *                  p_socket_data )
{
  auto self    = static_cast< ASync * >( p_user_data );         // CURLMOPT_SOCKETDATA
  auto context = static_cast< CurlContext * >( p_socket_data ); // curl_multi_assign
  //
  if ( self == nullptr ) // not possible
    return -1;
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
    self->destroy_curl_context( context ); // ok on nullptr
    break;
  default:
    break;
  }
  //
  return ok ? 0 : -1; // on error, all transfers currently in progress are aborted and fail
}

//--------------------------------------------------------------------
// Start UV loop and timer, and the worker thread waiting for IO
bool ASync::uv_init( void )
{
  m_uv_loop = uv_default_loop();
  //
  bool ok = true;
  //
  ok = ok && m_uv_loop != nullptr;
  ok = ok && 0 == uv_timer_init( m_uv_loop, &m_uv_timer );
  //
  if ( ok )
  {
    m_uv_timer.data = this;
    m_uv_running    = true;
    m_uv_worker     = std::thread(
        [ this ]
        {
          std::unique_lock lock( m_uv_run_mutex );
          //
          while ( m_uv_running )
            if ( uv_run( m_uv_loop, UV_RUN_NOWAIT ) )                  // if some requests are executing:
              lock.unlock(), uv_sleep( 0 ), lock.lock();               //    unlock, accept start_request, lock
            else                                                       // else
              m_uv_run_cv.wait_for( lock, std::chrono::seconds( 1 ) ); //    unlock, wait, start_request, lock
        } );
  }
  else
  {
    if ( m_uv_loop != nullptr )
    {
      uv_loop_close( m_uv_loop );
      m_uv_loop = nullptr;
    }
  }
  //
  return ok;
}

//--------------------------------------------------------------------
void ASync::uv_clear( void )
{
  m_uv_running = false;
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
        []( uv_handle_t * handle, [[maybe_unused]] void * arg )
        {
          if ( ! uv_is_closing( handle ) )
            uv_close( handle, nullptr );
        },
        nullptr );
    //
    while ( uv_run( m_uv_loop, UV_RUN_ONCE ) != 0 )
      uv_sleep( 10 );
    //
    while ( uv_loop_close( m_uv_loop ) == UV_EBUSY )
      uv_sleep( 10 );
    //
    m_uv_loop = nullptr;
  }
}

//--------------------------------------------------------------------
// Called by uv_run() when there is some data sent/received, inform multi,
// then check if the multi operation are finished.
// m_uv_run_mutex is locked.
void ASync::uv_io_cb( uv_poll_t * p_handle, [[maybe_unused]] int p_status, int p_events )
{
  int    flags   = 0;
  auto * context = static_cast< CurlContext * >( p_handle->data );
  //
  if ( context == nullptr ) // not possible
    return;
  //
  uv_timer_stop( &context->async.m_uv_timer );
  //
  if ( p_events & UV_READABLE )
    flags |= CURL_CSELECT_IN;
  if ( p_events & UV_WRITABLE )
    flags |= CURL_CSELECT_OUT;
  //
  int running_handles;
  curl_multi_socket_action( context->async.m_multi_handle, context->curl, flags, &running_handles ); // call multi_cb_socket
  //
  context->async.multi_fetch_messages(); // must be the last line as the wrapper can be deleted here
}

//--------------------------------------------------------------------
// Called by uv_run() when a timeout is triggered.
// m_uv_run_mutex is locked.
void ASync::uv_timeout_cb( uv_timer_t * p_handle )
{
  auto self = static_cast< ASync * >( p_handle->data );
  if ( self == nullptr ) // not possible
    return;
  //
  // Inform multi that a timeout occurred
  int running_handles;
  curl_multi_socket_action( self->m_multi_handle, CURL_SOCKET_TIMEOUT, 0, &running_handles ); // call multi_cb_socket
  //
  self->multi_fetch_messages(); // must be the last line as the wrapper can be deleted here
}

//--------------------------------------------------------------------
// The context is shared between multi and UV:
//  - as multi socket data (curl_multi_assign)
//  - in UV poll member data (here)
// m_uv_run_mutex is locked.
ASync::CurlContext * ASync::create_curl_context( curl_socket_t p_socket )
{
  CurlContext * context = new CurlContext( *this, p_socket );
  //
  if ( context != nullptr )
  {
    if ( uv_poll_init_socket( m_uv_loop, &context->poll, p_socket ) == 0 )
    {
      context->poll.data = context;
    }
    else
    {
      delete context;
      context = nullptr;
    }
  }
  //
  return context;
}

//--------------------------------------------------------------------
// Delete a context created by create_curl_context.
// Ok on nullptr.
// m_uv_run_mutex is locked.
void ASync::destroy_curl_context( CurlContext * p_context ) const
{
  if ( p_context != nullptr )
  {
    uv_close(
        reinterpret_cast<uv_handle_t *>( &p_context->poll ),
        []( uv_handle_t * handle )
        {
          if ( handle == nullptr ) // not possible
            return;
          //
          auto context = static_cast< CurlContext * >( handle->data );
          if ( context != nullptr ) // always true
            delete context;
        } );
  }
}

//--------------------------------------------------------------------
// Start the CB worker thread that will call the Wrapper callback
bool ASync::cb_init( void )
{
  m_cb_running = true;
  m_cb_worker  = std::thread(
      [ this ]
      {
        std::unique_lock lock( m_cb_mutex );
        //
        while ( m_cb_running )
        {
          while ( ! m_cb_queue.empty() )        // if some notifications are pending
          {
            auto [ wrapper, p_result_code ] = m_cb_queue.front();
            m_cb_queue.pop_front();
            lock.unlock();                      // retrieve the notification, unlock
            //
            wrapper->async_cb( p_result_code ); // notify: the Wrapper can be deleted here
            m_nb_running_requests--;
            //
            lock.lock();                        // lock
          }
          //
          m_cb_cv.wait_for( lock, std::chrono::seconds( 1 ) ); // unlock, wait, notify_wrapper, lock
        }
      } );
  //
  return true;
}

//--------------------------------------------------------------------
// Stop the CB worker thread
void ASync::cb_clear( void )
{
  m_cb_running = false;
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
  auto protocol_buffer = static_cast< std::string * >( p_userdata );
  auto to_add          = p_size * p_nmemb;
  //
  try {
    protocol_buffer->append( p_ptr, to_add );
  }
  catch ( ... ) {
    return CURL_WRITEFUNC_ERROR;
  }
  //
  return p_size * p_nmemb;
}

//--------------------------------------------------------------------
// To store header received during the transfer.
// Data is located in the Protocol and is a std::map< std::string, std::string >.
// m_uv_run_mutex is locked.
size_t ASync::curl_cb_header( const char * p_buffer, size_t p_size, size_t p_nitems, void * p_userdata )
{
  if ( p_userdata == nullptr )
    return CURL_WRITEFUNC_ERROR;
  //
  auto protocol_headers = static_cast< std::map< std::string, std::string > * >( p_userdata );
  auto line             = trim( std::string( p_buffer, p_size * p_nitems ) );
  auto colon            = line.find( ':' );
  //
  if ( colon != std::string::npos )
  {
    std::string key   = trim( line.substr( 0, colon  ) ); // key
    std::string value = trim( line.substr( colon + 1 ) ); // value
    //
    if ( ! key.empty() )
      protocol_headers->insert_or_assign( key, value );
  }
  //
  return p_nitems * p_size;
}

//--------------------------------------------------------------------
// Wait for all pending requests to finish.
// Wait a maximum of 30s, returns false if timeout is reached
bool ASync::wait_pending_requests( unsigned p_timeout_s ) const
{
  for ( unsigned attempts = p_timeout_s * 10; attempts > 0; attempts-- )
  {
    if ( m_nb_running_requests == 0 )
      return true;
    //
    uv_sleep( 100 );
  }
  //
  return false;
}

//--------------------------------------------------------------------
// Returns the operation outcome to the Wrapper.
// Ensure that the Wrapper is only called once (which should always be the case,
// except for abort_request).
// Depending on the config:
//  1] push the job in the queue, callback thread will call the Wrapper.
//  2] call the Wrapper from UV worker thread.
// m_uv_run_mutex is locked.
void ASync::notify_wrapper( CURL * p_curl, long p_result_code )
{
  WrapperBase * wrapper = nullptr;
  //
  if ( curl_easy_getinfo( p_curl, CURLINFO_PRIVATE, &wrapper ) == CURLE_OK )
  {
    if ( wrapper != nullptr ) // if not already notified
    {
      curl_easy_setopt( p_curl, CURLOPT_PRIVATE, nullptr ); // set when Wrapper start(), cleared here
      //
      if ( wrapper->m_threaded_cb ) // push it to CB queue
      {
        std::lock_guard lock( m_cb_mutex );
        m_cb_queue.push_back( std::make_tuple( wrapper, p_result_code ) );
        m_cb_cv.notify_one();
      }
      else // call it here
      {
        wrapper->async_cb( p_result_code ); // notify: the Wrapper can be deleted here
        m_nb_running_requests--;
      }
    }
  }
}

} // namespace curlev
