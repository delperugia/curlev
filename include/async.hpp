/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <curl/curl.h>
#include <deque>
#include <mutex>
#include <pthread.h>
#include <shared_mutex>
#include <thread>
#include <uv.h>

#include "authentication.hpp"
#include "certificates.hpp"
#include "options.hpp"

namespace curlev
{

class                                WrapperBase;
template < typename Protocol > class Wrapper;

//--------------------------------------------------------------------
// Because it is not possible to mix lock/unlock and lock_shared/unlock_shared
// in std::shared_mutex, and because libcurl CURLSHOPT_UNLOCKFUNC function
// does not receive the access, C functions must be used. They are wrapped
// here for conveniency (even if libcurl seldom use shared locks).
class shared_mutex
{
public:
  explicit shared_mutex()
  {
    m_initialized = pthread_rwlock_init( &m_lock, nullptr ) == 0;
  }
  //
  virtual ~shared_mutex()
  {
    if ( m_initialized )
      pthread_rwlock_destroy( &m_lock );
  }
  //
  void lock       ( void ) { pthread_rwlock_wrlock( &m_lock ); }
  void lock_shared( void ) { pthread_rwlock_rdlock( &m_lock ); }
  void unlock     ( void ) { pthread_rwlock_unlock( &m_lock ); }
  //
private:
  bool             m_initialized = false;
  pthread_rwlock_t m_lock;
};

//--------------------------------------------------------------------
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
  // Waits a maximum of p_timeout_ms milliseconds before forcefully stopping,
  // returns true if stopping was forced.
  bool stop( unsigned p_timeout_ms = 30000 );
  //
  // Accessors
  int  peak_requests   ( void ) const { return m_multi_running_max;     }
  int  active_requests ( void ) const { return m_multi_running_current; }
  bool protocol_crashed( void ) const { return m_protocol_has_crashed;  }
  //
  // Setting defaults
  bool options       ( const std::string & p_options );
  bool authentication( const std::string & p_credential );
  bool certificates  ( const std::string & p_certificates );
  //
protected:
  template < typename Protocol > friend class Wrapper;
  //
  // Retrieving defaults
  void get_default( Options & p_options, Authentication & p_authentication, Certificates & p_certificates ) const;
  //
  // Create a new easy handle that *must* be freed using return_handle
  [[nodiscard]] CURL * get_handle( WrapperBase * p_protocol ) const;
  //
  // Release a handle previously allocated by get_handle, ok on nullptr
  static
  void return_handle( CURL * & p_curl );
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
  // If crashes occurred while invoking protocol's callback
  std::atomic_bool m_protocol_has_crashed = false;
  //
  // Used by protocol classes as default values
  mutable std::shared_mutex m_default_locks;
  Options                   m_default_options;
  Authentication            m_default_authentication;
  Certificates              m_default_certificates;
  //
  // libcurl global
  //
  std::mutex  m_global_mutex;
  unsigned    m_global_count = 0;
  std::string m_global_ca_info; // default CURLINFO_CAINFO
  std::string m_global_ca_path; // default CURLINFO_CAPATH
  //
  bool global_init( void );
  void global_clear( void );
  //
  // libcurl share interface - share data between multiple easy handles (DNS, TLS...)
  //
  std::array< shared_mutex, CURL_LOCK_DATA_LAST > m_share_locks;
  CURLSH *                                        m_share_handle = nullptr;
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
  mutable std::mutex              m_uv_run_mutex;
  mutable std::condition_variable m_uv_run_cv;
  bool                            m_uv_running = false; // worker thread is running
  std::thread                     m_uv_worker;
  uv_loop_t *                     m_uv_loop = nullptr;
  uv_timer_t                      m_uv_timer;
  //
  bool        uv_init( void );
  void        uv_clear( void );
  void        uv_run_accept_requests( std::unique_lock< std::mutex > & p_lock ) const;
  void        uv_run_wait_requests  ( std::unique_lock< std::mutex > & p_lock ) const;
  static void uv_io_cb     ( uv_poll_t * p_handle, int p_status, int p_events );
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
    CurlContext( ASync & p_async, curl_socket_t p_curl ) :
      async( p_async ), curl( p_curl ){}
  };
  //
  CurlContext * create_curl_context ( curl_socket_t p_socket );
  static
  void          destroy_curl_context( CurlContext * p_context ); // cppcheck-suppress functionStatic
  //
  // Callback thread
  //
  using t_wrapper_shared_ptr_ptr = std::shared_ptr< WrapperBase > *;              // the Protocol object to call
  using t_cb_job                 = std::tuple< t_wrapper_shared_ptr_ptr, long >;  // the Protocol and the result
  //
  mutable std::mutex              m_cb_mutex;
  mutable std::condition_variable m_cb_cv;
  std::deque< t_cb_job >          m_cb_queue;
  bool                            m_cb_running = false; // worker thread is running
  std::thread                     m_cb_worker;
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
  bool wait_pending_requests( unsigned p_timeout_ms ) const;
  //
  // Returns the operation outcome to the wrapper, immediately or delayed
  void notify_wrapper( CURL * p_curl, long p_result_code );
  //
  // Call the wrapper, delete the shared_ptr
  void invoke_wrapper( t_wrapper_shared_ptr_ptr & p_wrapper, long p_result_code );
};

} // namespace curlev
