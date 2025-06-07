/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <atomic>
#include <deque>
#include <condition_variable>
#include <curl/curl.h>
#include <mutex>
#include <thread>
#include <uv.h>

namespace curlev
{

class                                WrapperBase;
template < typename Protocol > class Wrapper;

// This class handles the communications between libcurl multi and libuv.
// It is also here that libcurl easy handle are created.
// Create a single instance of this class, and call start before passing it
// to the various protocol creation functions.

class ASync
{
public:
  ASync();
  ~ASync();
  //
  // Must be called at least once before doing calling any other function of curlev
  bool start( void );
  //
  // Must be called at least when the program stops.
  // Waits a maximum of p_timeout_s seconds before forcefully stopping,
  // returns true if stopping was forced.
  bool stop( unsigned p_timeout_s = 30 );
  //
  // Accessors
  int peak_requests  ( void ) const { return m_multi_running_max; }
  int active_requests( void ) const { return m_multi_running_current; }
  //
protected:
  template < typename Protocol > friend class Wrapper;
  //
  // Create a new easy handle that *must* be freed using return_handle
  CURL * get_handle( void ) const;
  //
  // Previously retrieved by get_handle, ok on nullptr
  void return_handle( CURL * p_curl ) const; // cppcheck-suppress functionStatic
  //
  // Starts the transfer, ok on nullptr
  // If it fails the wrapper is notified with the curl result code
  bool start_request( CURL * p_curl, void * p_protocol_cb );
  //
  // Aborts a request previously started
  void abort_request( CURL * p_curl );
  //
private:
  //
  // Number of start_request waiting for m_uv_run_mutex
  std::atomic_long m_nb_waiting_requests = 0;
  //
  // Number of currently running (including waiting) requests, from start_request to post Wrapper notification
  std::atomic_long m_nb_running_requests = 0;
  //
  // libcurl share interface - share data between multiple easy handles (DNS, TLS...)
  //
  CURLSH *   m_share_handle = nullptr;
  std::mutex m_share_locks[ CURL_LOCK_DATA_LAST ];
  //
  bool        share_init( void );
  void        share_clear( void );
  static void share_cb_unlock( CURL * p_handle, curl_lock_data p_data, void * p_user_ptr );
  static void share_cb_lock(
      CURL *           p_handle,
      curl_lock_data   p_data,
      curl_lock_access p_access,
      void *           p_user_ptr );
  //
  // libcurl multi interface - enable multiple simultaneous transfers in the same thread
  //
  CURLM *         m_multi_handle          = nullptr;
  std::atomic_int m_multi_running_max     = 0; // maximum simultaneous requests reached
  std::atomic_int m_multi_running_current = 0; // current simultaneous request
  //
  bool       multi_init( void );
  void       multi_clear( void );
  void       multi_update_running_stats( int p_running_handles );
  void       multi_fetch_messages( void );
  static int multi_cb_timer( CURLM * p_multi, long p_timeout_ms, void * p_clientp );
  static int multi_cb_socket(
      CURL *        p_easy,
      curl_socket_t p_socket,
      int           p_what,
      void *        p_user_data,
      void *        p_socket_data );
  //
  // libuv - asynchronous I/O
  //
  mutable std::condition_variable m_uv_run_cv;
  mutable std::mutex              m_uv_run_mutex;
  bool                            m_uv_running = false; // worker thread is running
  std::thread                     m_uv_worker;
  uv_loop_t *                     m_uv_loop = nullptr;
  uv_timer_t                      m_uv_timer;
  //
  bool        uv_init( void );
  void        uv_clear( void );
  void        uv_run_accept_requests( std::unique_lock< std::mutex > & p_lock ) const;
  void        uv_run_wait_requests( std::unique_lock< std::mutex > & p_lock );
  static void uv_io_cb( uv_poll_t * p_handle, int p_status, int p_events );
  static void uv_timeout_cb( uv_timer_t * p_handle );
  //
  // Context shared between multi and uv
  //
  struct CurlContext
  {
    ASync &       async;
    curl_socket_t curl;
    uv_poll_t     poll;
    //
    CurlContext( ASync & p_async, curl_socket_t p_curl ) : async( p_async ), curl( p_curl ) {}
  };
  //
  CurlContext * create_curl_context ( curl_socket_t p_socket );
  void          destroy_curl_context( CurlContext * p_context ) const; // cppcheck-suppress functionStatic
  //
  // Callback thread
  //
  using t_wrapper_shared_ptr_ptr = std::shared_ptr< WrapperBase > *;
  using t_cb_job                 = std::tuple< t_wrapper_shared_ptr_ptr, long >;
  //
  mutable std::condition_variable           m_cb_cv;
  mutable std::mutex                        m_cb_mutex;
  std::deque< t_cb_job >                    m_cb_queue;
  bool                                      m_cb_running = false; // worker thread is running
  std::thread                               m_cb_worker;
  //
  bool cb_init( void );
  void cb_clear( void );
  //
  // To store data received during a transfer
  static size_t curl_cb_write( const char * p_ptr, size_t p_size, size_t p_nmemb, void * p_userdata );
  //
  // To store header received during the transfer
  static size_t curl_cb_header( const char * p_buffer, size_t p_size, size_t p_nitems, void * p_userdata );
  //
  // Wait for all pending requests to finish
  bool wait_pending_requests( unsigned p_timeout_s ) const;
  //
  // Returns the operation outcome to the wrapper, immediately or delayed
  void notify_wrapper( CURL * p_curl, long p_result_code );
  //
  // Call the wrapper, delete the shared_ptr
  void invoke_wrapper( t_wrapper_shared_ptr_ptr & p_wrapper, long p_result_code );
};

} // namespace curlev
