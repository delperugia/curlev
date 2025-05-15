#include "common.hpp"
#include "http.hpp"

HTTP::~HTTP()
{
    release_curl_extras();
}

// Add a header to the next request.
HTTP & HTTP::add_header( const std::string & p_header, const std::string & p_value )
{
    m_request_headers.insert_or_assign( p_header, p_value );
    //
    return *this;
}

// Add headers to the next request.
HTTP & HTTP::add_headers( t_key_values p_headers )
{
    m_request_headers.insert( p_headers.begin(), p_headers.end() );
    //
    return *this;
}

// Sending a raw body using POST.
HTTP & HTTP::do_post( const std::string &  p_url,
                      const std::string &  p_content_type,
                      const std::string &  p_body,
                      const t_key_values & p_query_parameters )
{
    return request_with_body( "POST", p_url, p_content_type, p_body, p_query_parameters );
}

// Sending a raw body using PUT.
HTTP & HTTP::do_put( const std::string &  p_url,
                     const std::string &  p_content_type,
                     const std::string &  p_body,
                     const t_key_values & p_query_parameters )
{
    return request_with_body( "PUT", p_url, p_content_type, p_body, p_query_parameters );
}

// Sending a raw body using PATCH.
HTTP & HTTP::do_patch( const std::string &  p_url,
                       const std::string &  p_content_type,
                       const std::string &  p_body,
                       const t_key_values & p_query_parameters )
{
    return request_with_body( "PATCH", p_url, p_content_type, p_body, p_query_parameters );
}

// Sending parameters in body using POST.
HTTP & HTTP::do_post( const std::string &  p_url,
                      const t_key_values & p_body_parameter,
                      const t_key_values & p_query_parameters )
{
    return request_with_body( "POST",
                              p_url,
                              "application/x-www-form-urlencoded",
                              encode_parameters( p_body_parameter ),
                              p_query_parameters );
}

// Sending parameters in body using PUT.
HTTP & HTTP::do_put( const std::string &  p_url,
                     const t_key_values & p_body_parameter,
                     const t_key_values & p_query_parameters )
{
    return request_with_body( "PUT",
                              p_url,
                              "application/x-www-form-urlencoded",
                              encode_parameters( p_body_parameter ),
                              p_query_parameters );
}

// Sending parameters in body using PATCH.
HTTP & HTTP::do_patch( const std::string &  p_url,
                       const t_key_values & p_body_parameter,
                       const t_key_values & p_query_parameters )
{
    return request_with_body( "PATCH",
                              p_url,
                              "application/x-www-form-urlencoded",
                              encode_parameters( p_body_parameter ),
                              p_query_parameters );
}

// Sending a MIME body using POST.
HTTP & HTTP::do_post( const std::string &  p_url,
                      const t_mime_parts & p_mime_parameters,
                      const t_key_values & p_query_parameters )
{
    return request_with_mime_body( "POST", p_url, p_mime_parameters, p_query_parameters );
}

// Sending a MIME body using PUT.
HTTP & HTTP::do_put( const std::string &  p_url,
                     const t_mime_parts & p_mime_parameters,
                     const t_key_values & p_query_parameters )
{
    return request_with_mime_body( "PUT", p_url, p_mime_parameters, p_query_parameters );
}

// Sending a MIME body using PATCH.
HTTP & HTTP::do_patch( const std::string &  p_url,
                       const t_mime_parts & p_mime_parameters,
                       const t_key_values & p_query_parameters )
{
    return request_with_mime_body( "PATCH", p_url, p_mime_parameters, p_query_parameters );
}

// Calling using a GET.
HTTP & HTTP::do_get( const std::string & p_url, const t_key_values & p_query_parameters )
{
    return request_no_body( "GET", p_url, p_query_parameters );
}

// Calling using a DELETE.
HTTP & HTTP::do_delete( const std::string & p_url, const t_key_values & p_query_parameters )
{
    return request_no_body( "DELETE", p_url, p_query_parameters );
}

// Clear all previous configuration before doing a new request.
HTTP & HTTP::clear( void )
{
    release_curl_extras();
    //
    m_request_headers.clear();
    m_request_body.clear();
    //
    m_response_headers.clear();
    m_response_content_type.clear();
    m_response_redirect_url.clear();
    m_response_body.clear();
    //
    Wrapper< HTTP >::clear();
    //
    return *this;
}

// Called by Wrapper after a request to retrieve protocol specific details.
// Release extra curl handles that were used during the operation.
void HTTP::finalize( void )
{
    char * value;
    //
    if ( curl_easy_getinfo( m_curl, CURLINFO_CONTENT_TYPE, &value ) == CURLE_OK && value != nullptr )
        m_response_content_type = value;
    //
    if ( curl_easy_getinfo( m_curl, CURLINFO_REDIRECT_URL, &value ) == CURLE_OK && value != nullptr )
        m_response_redirect_url = value;
    //
    release_curl_extras();
}

// All requests without body.
HTTP & HTTP::request_no_body( const std::string & p_method,
                              const std::string & p_url,
                              const t_key_values &  p_query_parameters )
{
    if ( prepare_curl( p_method, p_url, p_query_parameters ) )  // set m_response_code on error
    {
        bool ok = true;
        //
        ok = ok && fill_headers();  // set m_curl_headers, m_response_code on error
        //
        if ( ! ok && m_response_code == c_code_success )
            m_response_code = c_code_error_http_prepare_no_body;
    }
    //
    return *this;
}

// Requests with raw or parameters bodies.
HTTP & HTTP::request_with_body( const std::string & p_method,
                                const std::string & p_url,
                                const std::string & p_content_type,
                                const std::string & p_body,
                                const t_key_values &  p_query_parameters )
{
    if ( prepare_curl( p_method, p_url, p_query_parameters ) )  // set m_response_code on error
    {
        m_request_body = p_body;
        m_request_headers.insert_or_assign( "Content-Type", p_content_type );
        //
        bool ok = true;
        //
        ok = ok && CURLE_OK == curl_easy_setopt( m_curl, CURLOPT_POSTFIELDS   , m_request_body.c_str() ); // must be persistent
        ok = ok && CURLE_OK == curl_easy_setopt( m_curl, CURLOPT_POSTFIELDSIZE, p_body.size()          ); // will add the Content-Length header
        //
        ok = ok && fill_headers();  // set m_curl_headers, m_response_code on error
        //
        if ( ! ok && m_response_code == c_code_success )
            m_response_code = c_code_error_http_prepare_body;
    }
    //
    return *this;
}

// Request with MIME body.
HTTP & HTTP::request_with_mime_body( const std::string & p_method,
                                     const std::string & p_url,
                                     const t_mime_parts &  p_mime_parameters,
                                     const t_key_values &  p_query_parameters )
{
    if ( prepare_curl( p_method, p_url, p_query_parameters ) )  // set m_response_code on error
    {
        bool ok = true;
        //
        ok = ok && fill_mime( p_mime_parameters );  // set m_curl_mime, m_response_code on error
        ok = ok && CURLE_OK == curl_easy_setopt( m_curl, CURLOPT_MIMEPOST, m_curl_mime );  // will add Content-Type and Content-Length headers
        //
        ok = ok && fill_headers();  // set m_curl_headers, m_response_code on error
        //
        if ( ! ok && m_response_code == c_code_success )
            m_response_code = c_code_error_http_prepare_mime;
    }
    //
    return *this;
}

// Preparing curl before the Wrapper::start.
bool HTTP::prepare_curl( const std::string &  p_method,
                         const std::string &  p_url,
                         const t_key_values & p_query_parameters )
{
    if ( m_response_code != c_code_success )
        return false;
    //
    m_response_headers.clear();
    m_response_content_type.clear();
    m_response_redirect_url.clear();
    m_response_body.clear();
    //
    m_request_headers.insert_or_assign( "Expect", "" ); // to prevent cURL to send Expect
    //
    bool ok = true;
    //
    ok = ok && CURLE_OK == curl_easy_setopt( m_curl, CURLOPT_WRITEDATA    , & m_response_body );
    ok = ok && CURLE_OK == curl_easy_setopt( m_curl, CURLOPT_URL          , url_with_parameters( p_url, p_query_parameters ).c_str() );
    ok = ok && CURLE_OK == curl_easy_setopt( m_curl, CURLOPT_CUSTOMREQUEST, p_method.c_str() );
    //
    ok = ok && Wrapper< HTTP >::prepare_curl();
    //
    if ( ok )
        return true;
    //
    m_response_code = c_code_error_http_prepare;
    return false;
}

// Release extra curl handles that were used during the operation.
void HTTP::release_curl_extras( void )
{
    curl_slist_free_all( m_curl_headers ); 
    curl_mime_free     ( m_curl_mime );    
    m_curl_headers = nullptr;
    m_curl_mime    = nullptr;
}

// Add all headers to the request.
bool HTTP::fill_headers( void )
{
    curl_slist_free_all( m_curl_headers );
    m_curl_headers = nullptr;
    //
    bool ok = true;
    //
    for ( const auto & header : m_request_headers )
        ok = ok && curl_slist_checked_append( m_curl_headers, header.first + ": " + header.second );
    //
    ok = ok && CURLE_OK == curl_easy_setopt( m_curl, CURLOPT_HTTPHEADER, m_curl_headers );
    //
    if ( ! ok )
    {
        curl_slist_free_all( m_curl_headers );
        m_curl_headers  = nullptr;
        m_response_code = c_code_error_http_headers;
    }
    //
    return ok;
}

namespace
{
    // Add a MIME part containing a single parameter
    bool add_mime_parameter( curl_mimepart * p_mime_part, const HTTP::t_mime_parameter & p_parameter )
    {
        const auto & [ name, value ] = p_parameter;
        bool ok                      = true;
        //
        ok = ok && CURLE_OK == curl_mime_name( p_mime_part, name.c_str() );
        ok = ok && CURLE_OK == curl_mime_data( p_mime_part, value.c_str(), value.length() );
        //
        return ok;
    }

    // Add a MIME part containing a document
    bool add_mime_file( curl_mimepart * p_mime_part, const HTTP::t_mime_file & p_file )
    {
        const auto & [ name, type, data, filename ] = p_file;
        bool ok                                     = true;
        //
        ok = ok && CURLE_OK == curl_mime_name( p_mime_part, name.c_str() );
        ok = ok && CURLE_OK == curl_mime_type( p_mime_part, type.c_str() );
        ok = ok && CURLE_OK == curl_mime_data( p_mime_part, data.data(), data.size() );
        //
        ok = ok && CURLE_OK == curl_mime_filename( p_mime_part, filename.c_str() );
        //
        return ok;
    }
}

// Build the MIME body.
bool HTTP::fill_mime( const t_mime_parts &  p_mime_parameters )
{
    curl_mime_free( m_curl_mime );
    m_curl_mime = curl_mime_init( m_curl );
    //
    bool ok = true;
    //
    ok = ok && m_curl_mime != nullptr;
    //
    if ( ok )
        for ( const auto & parameter : p_mime_parameters )
        {
            curl_mimepart * mime_part = curl_mime_addpart( m_curl_mime );
            if ( mime_part == nullptr )
            {
                ok = false;
                break;
            }
            //
            std::visit(
                [ & ok, & mime_part ]( auto && part )
                {
                    using T = std::decay_t< decltype( part ) >;
                    //
                    if constexpr ( std::is_same_v< T, t_mime_file > )
                        ok = ok && add_mime_file( mime_part, part );
                    else if constexpr ( std::is_same_v< T, t_mime_parameter > )
                        ok = ok && add_mime_parameter( mime_part, part );
                    else
                        ok = false;
                },
                parameter );
        }
    //
    if ( ! ok )
    {
        curl_mime_free( m_curl_mime );
        m_curl_mime     = nullptr;
        m_response_code = c_code_error_http_mime;
    }
    //
    return ok;
}

// Encode parameters into a application/x-www-form-urlencoded string.
std::string HTTP::encode_parameters( const t_key_values & p_parameters )
{
    std::string encoded;
    //
    for ( const auto & [ key, value ] : p_parameters )
    {
        char * enc_key = curl_easy_escape( m_curl, key.c_str()  , 0 );
        char * enc_val = curl_easy_escape( m_curl, value.c_str(), 0 );
        //
        if ( ! encoded.empty() )
            encoded += "&";
        //
        encoded += enc_key + std::string( "=" ) + enc_val;
        //
        curl_free( enc_key );
        curl_free( enc_val );
    }
    //
    return encoded;
}

// Add parameters as query parameters to the given URL.
std::string HTTP::url_with_parameters( const std::string & p_url, const t_key_values & p_parameters )
{
    std::string new_url        = p_url;
    std::string url_parameters = encode_parameters( p_parameters );
    //
    if ( ! url_parameters.empty() )
    {
        if ( size_t pos = p_url.find_last_of( '?' ); pos == std::string::npos )
            new_url += "?" + url_parameters;
        else
            new_url += "&" + url_parameters;
    }
    //
    return new_url;
}