#include <cstring>

#include "async.hpp"
#include "http.hpp"

ASync::ASync()
{
    m_uv_timer.data = nullptr;
}

ASync::~ASync()
{
    stop();
}

// Start curl, share and multi, then uv and its worker thread
bool ASync::start()
{
    stop();
    //
    bool ok = true;
    //
    ok = ok && CURLE_OK == curl_global_init( CURL_GLOBAL_ALL );
    ok = ok && share_init();
    ok = ok && multi_init();
    ok = ok && uv_init();
    //
    return ok;
}

// Stop the worker thread used by uv, share and multi of curl.
// Ensure all uv handles are released.
// Clean curl.
void ASync::stop()
{
    m_uv_running = false;
    //
    if ( m_uv_worker.joinable() )
        m_uv_worker.join();
    //
    if ( m_uv_timer.data != nullptr ) // set to this in start: it is used to know if uv_timer_init was called
    {
        uv_timer_stop( & m_uv_timer );
        m_uv_timer.data = nullptr;
    }
    //
    curl_share_cleanup( m_share_handle ); // ok on nullptr 
    m_share_handle = nullptr;
    //
    curl_multi_cleanup( m_multi_handle ); // ok on nullptr
    m_multi_handle = nullptr;
    //
    if ( m_uv_loop != nullptr )
    {
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
    //
    curl_global_cleanup();
}

// Create a new easy handle that *must* be freed using return_handle
// CURLOPT_WRITEDATA is properly set in the wrapper to one of its member
CURL * ASync::get_handle() const
{
    auto curl = curl_easy_init();
    bool ok   = true;
    //
    ok = ok && curl != nullptr;
    ok = ok && CURLE_OK == curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, curl_cb_write  );
    ok = ok && CURLE_OK == curl_easy_setopt( curl, CURLOPT_WRITEDATA    , nullptr        );
    ok = ok && CURLE_OK == curl_easy_setopt( curl, CURLOPT_SHARE        , m_share_handle );
    ok = ok && CURLE_OK == curl_easy_setopt( curl, CURLOPT_NOSIGNAL     , 1L             );
    //
    if ( ! ok )
    {
        curl_easy_cleanup( curl ); // ok on nullptr
        return nullptr;
    }
    //
    return curl;
}

// Previously retrieved by get_handle, ok on nullptr
void ASync::return_handle( CURL * p_curl ) const
{
    curl_easy_cleanup( p_curl ); // ok on nullptr
}

// Starts the transfer, ok on nullptr
// If it fails the wrapper is notified with the curl result code
void ASync::start_request( CURL * p_curl )
{
    CURLMcode result_code;
    //
    {
        std::lock_guard lock( m_uv_run_mutex );
        //
        result_code = curl_multi_add_handle( m_multi_handle, p_curl );  // ok on nullptr, added at the end of the current uv_run
    }
    //
    if ( result_code == CURLM_OK )
        m_uv_run_cv.notify_one();
    else
        notify_wrapper( p_curl, result_code );
}

bool ASync::share_init( void )
{
    m_share_handle = curl_share_init();
    //
    bool ok = true;
    //
    ok = ok && m_share_handle != nullptr;
    ok = ok && CURLSHE_OK == curl_share_setopt( m_share_handle, CURLSHOPT_SHARE,      CURL_LOCK_DATA_DNS         );
    ok = ok && CURLSHE_OK == curl_share_setopt( m_share_handle, CURLSHOPT_SHARE,      CURL_LOCK_DATA_SSL_SESSION );
    ok = ok && CURLSHE_OK == curl_share_setopt( m_share_handle, CURLSHOPT_SHARE,      CURL_LOCK_DATA_CONNECT     );
    ok = ok && CURLSHE_OK == curl_share_setopt( m_share_handle, CURLSHOPT_LOCKFUNC,   &share_cb_lock             );
    ok = ok && CURLSHE_OK == curl_share_setopt( m_share_handle, CURLSHOPT_UNLOCKFUNC, &share_cb_unlock           );
    ok = ok && CURLSHE_OK == curl_share_setopt( m_share_handle, CURLSHOPT_USERDATA,   this                       );
    //
    if ( ! ok )
    {
        curl_share_cleanup( m_share_handle ); // ok on nullptr
        m_share_handle = nullptr;
    }
    //
    return ok;
}

// Lock some of curl share data
void ASync::share_cb_lock( [[maybe_unused]] CURL *           p_handle,
                                            curl_lock_data   p_data,
                           [[maybe_unused]] curl_lock_access p_access,
                           void *                            p_user_ptr )
{
    auto self = static_cast< ASync * >( p_user_ptr );
    //
    self->m_share_locks[ p_data ].lock();
}

// Unlock some of curl share data
void ASync::share_cb_unlock( [[maybe_unused]] CURL *         p_handle,
                                              curl_lock_data p_data,
                             void *                          p_user_ptr )
{
    auto self = static_cast< ASync * >( p_user_ptr );
    //
    self->m_share_locks[ p_data ].unlock();
}

bool ASync::multi_init( void )
{
    m_multi_handle = curl_multi_init();
    //
    bool ok = true;
    //
    ok = ok && m_multi_handle != nullptr;
    ok = ok && CURLM_OK == curl_multi_setopt( m_multi_handle, CURLMOPT_SOCKETFUNCTION, &multi_cb_socket );
    ok = ok && CURLM_OK == curl_multi_setopt( m_multi_handle, CURLMOPT_TIMERFUNCTION , &multi_cb_timer  );
    ok = ok && CURLM_OK == curl_multi_setopt( m_multi_handle, CURLMOPT_SOCKETDATA    , this             );
    ok = ok && CURLM_OK == curl_multi_setopt( m_multi_handle, CURLMOPT_TIMERDATA     , this             ); 
    //
    if ( ! ok )
    {
        curl_multi_cleanup( m_multi_handle ); // ok on nullptr
        m_multi_handle = nullptr;
    }
    //
    return ok;
}

namespace
{

// Find the result to send to the wrapper: the libcurl error code if transfer failed,
// or the protocol result code if transfer succeeded
long outcome_code( CURLMsg * p_message )
{
    long result_code;
    //
    if ( p_message->data.result == CURLE_OK )
    {
        auto protocol_code = curl_easy_getinfo( p_message->easy_handle, CURLINFO_RESPONSE_CODE, &result_code );
        //
        if ( protocol_code != CURLE_OK )
            result_code = protocol_code;
    }
    else
        result_code = p_message->data.result;
    //
    return result_code;
}

}  // namespace

// Check if some multi operation are completed.
// Remove them and notify the wrapper.s
void  ASync::multi_fetch_messages( void )
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
                notify_wrapper( message->easy_handle, outcome_code( message ) );  // must be the last line as the wrapper can be deleted here, easy_handle may be invalid
                break;
            default:
                break;
        }
    }
}

// Called by uv to start and stop timers
void ASync::multi_cb_timer( [[maybe_unused]] CURLM * p_multi, long p_timeout_ms, void * p_clientp )
{
    auto self1 = static_cast< ASync * >( p_clientp );
    if ( self1 == nullptr )  // not possible
        return;
    //
    if ( p_timeout_ms < 0 )  // delete the timer
    {
        uv_timer_stop( &self1->m_uv_timer );
    }
    else
    {
        uv_timer_start(
            &self1->m_uv_timer,
            []( uv_timer_t * handle )
            {
                auto self2 = static_cast< ASync * >( handle->data );
                if ( self2 == nullptr )  // not possible
                    return;
                //
                int running_handles;
                curl_multi_socket_action( self2->m_multi_handle, CURL_SOCKET_TIMEOUT, 0, &running_handles ); // inform multi that a timeout occurred
                //
                self2->multi_fetch_messages();  // must be the last line as the wrapper can be deleted here
            },
            static_cast< uint64_t >( p_timeout_ms ),
            0 );
    }
}

// Called when there is an update on one of multi socket. Either
// we expect data (and call uv_poll_start) or are done (and call uv_poll_stop).
int ASync::multi_cb_socket( [[maybe_unused]] CURL * p_easy,
                            curl_socket_t           p_socket,
                            int                     p_what,
                            void *                  p_user_data,
                            void *                  p_socket_data )
{
    auto self    = static_cast< ASync *       >( p_user_data );    // CURLMOPT_SOCKETDATA
    auto context = static_cast< CurlContext * >( p_socket_data );  // curl_multi_assign
    //
    if ( self == nullptr )  // not possible
        return -1;
    //
    switch ( p_what )
    {
        case CURL_POLL_IN:
        case CURL_POLL_OUT:
        case CURL_POLL_INOUT:
            if ( context == nullptr )
                context = self->create_curl_context( p_socket );
            //
            if ( context != nullptr )
            {
                curl_multi_assign( self->m_multi_handle, p_socket, context );
                //
                int events = 0;
                //
                if ( p_what != CURL_POLL_IN )
                    events |= UV_WRITABLE;
                if ( p_what != CURL_POLL_OUT )
                    events |= UV_READABLE;
                //
                if ( uv_poll_start( &context->poll, events, &uv_cb ) == 0 )
                    {;}
            }
            break;
        case CURL_POLL_REMOVE:
            if ( context != nullptr )
            {
                if ( uv_poll_stop( &context->poll ) == 0 )
                    {;}
                //
                curl_multi_assign( self->m_multi_handle, p_socket, nullptr ); // clear the context reference
                //
                self->destroy_curl_context( context );
            }
            break;
        default: break;
    }
    //
    return 0;
}

// Start uv loop and timer, and the worker thread waiting for IO.
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
                    if ( uv_run( m_uv_loop, UV_RUN_NOWAIT ) )                    // if some requests are executing:
                        lock.unlock(), uv_sleep( 0 ), lock.lock();               //    unlock, accept start_request, lock
                    else                                                         // else
                        m_uv_run_cv.wait_for( lock, std::chrono::seconds( 1 ) ); //    unlock, wait start_request, lock
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

// Called by uv when there is some data sent/received, inform multi,
// then check if the multi operation are finished
void ASync::uv_cb( uv_poll_t * p_handle, [[maybe_unused]] int p_status, int p_events )
{
    int    running_handles;
    int    flags   = 0;
    auto * context = static_cast< CurlContext * >( p_handle->data );
    //
    if ( context == nullptr )  // not possible
        return;
    //
    uv_timer_stop( &context->async->m_uv_timer );
    //
    if ( p_events & UV_READABLE )
        flags |= CURL_CSELECT_IN;
    if ( p_events & UV_WRITABLE )
        flags |= CURL_CSELECT_OUT;
    //
    auto mc = curl_multi_socket_action( context->async->m_multi_handle, context->curl, flags, &running_handles );
    //
    context->async->multi_fetch_messages();  // must be the last line as the wrapper can be deleted here
}

// The context is shared between multi and uv:
//  - as multi socket data (curl_multi_assign)
//  - in uv poll data (here)
ASync::CurlContext * ASync::create_curl_context( curl_socket_t p_socket )
{
    CurlContext * context = new CurlContext;
    //
    if ( context != nullptr )
    {
        context->async = this;
        context->curl  = p_socket;
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
    }
    //
    return context;
}

// Delete a context created by create_curl_context.
// Ok on nullptr.
// This function is running asynchronously.
void ASync::destroy_curl_context( CurlContext * p_context ) const
{
    if ( p_context != nullptr )
        uv_close( reinterpret_cast< uv_handle_t * >( &p_context->poll ),
                    []( uv_handle_t * handle )
                    {
                        if ( handle == nullptr )  // not possible
                            return;
                        //
                        auto context = static_cast< CurlContext * >( handle->data );
                        if ( context != nullptr )  // always true
                            delete context;
                    } );
}

// To store data received during the transfer
size_t ASync::curl_cb_write( const char * p_ptr, size_t p_size, size_t p_nmemb, void * p_userdata )
{
    if ( p_userdata == nullptr )
        return CURL_WRITEFUNC_ERROR;
    //
    auto   output       = static_cast< std::string * >( p_userdata );
    size_t current_size = output->size();
    size_t to_add       = p_size * p_nmemb;
    //
    output->resize( current_size + to_add );
    memcpy( output->data() + current_size, p_ptr, to_add );
    //
    return p_size * p_nmemb;
}

// Returns the operation outcome to the wrapper
void ASync::notify_wrapper( CURL * p_curl, long p_result_code ) const
{
    WrapperBase * wrapper = nullptr;
    //
    if ( curl_easy_getinfo( p_curl, CURLINFO_PRIVATE, &wrapper ) == CURLE_OK && wrapper != nullptr )
        wrapper->back( p_result_code );  // must be the last line as the wrapper can be deleted here
}
