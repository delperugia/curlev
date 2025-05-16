#pragma once

#include <condition_variable>
#include <curl/curl.h>
#include <mutex>
#include <thread>
#include <uv.h>

template< typename Protocol >
class Wrapper;

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
        bool   start( void );
        void   stop( void );
        //
    protected:
        template< typename Protocol >
        friend class Wrapper;
        //
        // Create a new easy handle that *must* be freed using return_handle
        CURL * get_handle( void ) const;
        //
        // Previously retrieved by get_handle, ok on nullptr
        void   return_handle( CURL * p_curl ) const; // cppcheck-suppress functionStatic
        //
        // Starts the transfer, ok on nullptr
        // If it fails the wrapper is notified with the curl result code
        void   start_request( CURL * p_curl );
        //
    private:
        // libcurl share interface - share data between multiple easy handles (DNS, TLS...)
        //
        CURLSH *   m_share_handle = nullptr;
        std::mutex m_share_locks[ CURL_LOCK_DATA_LAST ];
        //
        bool        share_init( void );
        static void share_cb_lock( CURL *           p_handle,
                                   curl_lock_data   p_data,
                                   curl_lock_access p_access,
                                   void *           p_user_ptr );

        static void share_cb_unlock( CURL *         p_handle,
                                     curl_lock_data p_data,
                                     void *         p_user_ptr );
        //
        // libcurl multi interface - enable multiple simultaneous transfers in the sam thread
        //
        CURLM * m_multi_handle = nullptr;
        //
        bool        multi_init( void );
        void        multi_fetch_messages( void );
        static void multi_cb_timer ( CURLM *       p_multi,
                                     long          p_timeout_ms,
                                     void *        p_clientp );
        static int  multi_cb_socket( CURL *        p_easy,
                                     curl_socket_t p_socket,
                                     int           p_what,
                                     void *        p_user_data,
                                     void *        p_socket_data );
        //
        // libuv - asynchronous I/O
        //
        std::condition_variable m_uv_run_cv;
        std::mutex              m_uv_run_mutex;
        bool                    m_uv_running   = false; // worker thread is running
        std::thread             m_uv_worker;
        uv_loop_t *             m_uv_loop      = nullptr;
        uv_timer_t              m_uv_timer;
        //
        bool          uv_init( void );
        static void   uv_cb( uv_poll_t * p_handle, int p_status, int p_events );
        //
        // Context shared between multi and uv
        //
        struct CurlContext
        {
            uv_poll_t     poll;
            ASync *       async;
            curl_socket_t curl;
        };
        //
        CurlContext * create_curl_context ( curl_socket_t p_socket  );
        void          destroy_curl_context( CurlContext * p_context ) const; // cppcheck-suppress functionStatic
        //
        // To store data received during the transfer
        static size_t curl_cb_write( const char * p_ptr, size_t p_size, size_t p_nmemb, void * p_userdata );
        //
        // Returns the operation outcome to the wrapper
        void notify_wrapper( CURL * p_curl, long p_result_code ) const; // cppcheck-suppress functionStatic
};
