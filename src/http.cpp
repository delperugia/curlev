/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "common.hpp"
#include "http.hpp"

namespace curlev
{

// Some magic values: the average "parameter=value" length
constexpr auto c_average_parameter_length = 32;

//--------------------------------------------------------------------
HTTP::~HTTP()
{
  release_curl_extras();
}

//--------------------------------------------------------------------
// Calling using a GET
HTTP & HTTP::GET( const std::string & p_url, const t_key_values & p_query_parameters )
{
  if ( is_idle() )
  {
    clear();
    //
    m_request_method = Method::eGET;
    m_request_url    = p_url;
    m_request_query_parameters.insert( p_query_parameters.begin(), p_query_parameters.end() );
  }
  //
  return *this;
}

//--------------------------------------------------------------------
// Calling using a DELETE
HTTP & HTTP::DELETE( const std::string & p_url, const t_key_values & p_query_parameters )
{
  if ( is_idle() )
  {
    clear();
    //
    m_request_method = Method::eDELETE;
    m_request_url    = p_url;
    m_request_query_parameters.insert( p_query_parameters.begin(), p_query_parameters.end() );
  }
  //
  return *this;
}

//--------------------------------------------------------------------
// Sending a raw body using POST
HTTP & HTTP::POST(
    const std::string & p_url,
    const std::string & p_content_type,
    const std::string & p_body )
{
  if ( is_idle() )
  {
    clear();
    //
    m_request_method       = Method::ePOST;
    m_request_url          = p_url;
    m_request_body         = p_body;
    m_request_content_type = p_content_type;
  }
  //
  return *this;
}

//--------------------------------------------------------------------
// Sending a raw body using PUT
HTTP & HTTP::PUT(
    const std::string & p_url,
    const std::string & p_content_type,
    const std::string & p_body )
{
  if ( is_idle() )
  {
    clear();
    //
    m_request_method       = Method::ePUT;
    m_request_url          = p_url;
    m_request_body         = p_body;
    m_request_content_type = p_content_type;
  }
  //
  return *this;
}

//--------------------------------------------------------------------
// Sending a raw body using PATCH
HTTP & HTTP::PATCH(
    const std::string & p_url,
    const std::string & p_content_type,
    const std::string & p_body )
{
  if ( is_idle() )
  {
    clear();
    //
    m_request_method       = Method::ePATCH;
    m_request_url          = p_url;
    m_request_body         = p_body;
    m_request_content_type = p_content_type;
  }
  //
  return *this;
}

//--------------------------------------------------------------------
// Sending url encoded parameters in the body using POST
HTTP & HTTP::POST( const std::string & p_url, const t_key_values & p_body_parameter )
{
  if ( is_idle() )
  {
    clear();
    //
    m_request_method = Method::ePOST;
    m_request_url    = p_url;
    m_request_body_parameters.insert( p_body_parameter.begin(), p_body_parameter.end() );
  }
  //
  return *this;
}

//--------------------------------------------------------------------
// Sending url encoded parameters in the body using PUT
HTTP & HTTP::PUT( const std::string & p_url, const t_key_values & p_body_parameter )
{
  if ( is_idle() )
  {
    clear();
    //
    m_request_method = Method::ePUT;
    m_request_url    = p_url;
    m_request_body_parameters.insert( p_body_parameter.begin(), p_body_parameter.end() );
  }
  //
  return *this;
}

//--------------------------------------------------------------------
// Sending url encoded parameters in the body using PATCH
HTTP & HTTP::PATCH( const std::string & p_url, const t_key_values & p_body_parameter )
{
  if ( is_idle() )
  {
    clear();
    //
    m_request_method = Method::ePATCH;
    m_request_url    = p_url;
    m_request_body_parameters.insert( p_body_parameter.begin(), p_body_parameter.end() );
  }
  //
  return *this;
}

//--------------------------------------------------------------------
// Add headers to the request
HTTP & HTTP::add_headers( const t_key_values & p_headers )
{
  if ( is_idle() )
    m_request_headers.insert( p_headers.begin(), p_headers.end() );
  //
  return *this;
}

//--------------------------------------------------------------------
// Add query parameters to the request
HTTP & HTTP::add_query_parameters( const t_key_values & p_query_parameters )
{
  if ( is_idle() )
    m_request_query_parameters.insert( p_query_parameters.begin(), p_query_parameters.end() );
  //
  return *this;
}

//--------------------------------------------------------------------
// Add body parameters to the request.
// Do not use with MIME nor raw body requests.
HTTP & HTTP::add_body_parameters( const t_key_values & p_body_parameter )
{
  if ( is_idle() )
    m_request_body_parameters.insert( p_body_parameter.begin(), p_body_parameter.end() );
  //
  return *this;
}

//--------------------------------------------------------------------
// Add MIME parts to th ebody of the request.
// Only for a request without a raw body or parameters.
HTTP & HTTP::add_mime_parameters( const mime::parts & p_parts )
{
  if ( is_idle() )
    m_request_mime.add_parts( p_parts );
  //
  return *this;
}

//--------------------------------------------------------------------
// Accessors to be used after a request
t_key_values_ci HTTP::get_headers     ( void ) const noexcept { return is_running() ? t_key_values_ci() : m_response_headers;      }
std::string     HTTP::get_content_type( void ) const noexcept { return is_running() ? ""                : m_response_content_type; }
std::string     HTTP::get_redirect_url( void ) const noexcept { return is_running() ? ""                : m_response_redirect_url; }
std::string     HTTP::get_body        ( void ) const noexcept { return is_running() ? ""                : m_response_body;         }

//--------------------------------------------------------------------
std::future< HTTP::Response > HTTP::launch( void )
{
  auto promise = std::make_shared< std::promise< HTTP::Response > >();
  auto future  = promise->get_future();
  //
  threaded_callback( false ). // because the callback in start() is fast
  start(
      [ promise ]( const auto & http )
      {
        Response response;
        response.code         = http.get_code();
        response.headers      = http.get_headers();
        response.redirect_url = http.get_redirect_url();
        response.content_type = http.get_content_type();
        response.body         = http.get_body();
        //
        promise->set_value( response );
      } );
  //
  return future;
}

//--------------------------------------------------------------------
// Called by Wrapper before starting a request.
// It is guaranteed that there is no operation running.
bool HTTP::prepare_protocol( void )
{
  m_response_headers     .clear();
  m_response_redirect_url.clear();
  m_response_content_type.clear();
  m_response_body        .clear();
  //
  m_request_headers.insert_or_assign( "Expect", "" ); // to prevent libcurl to send Expect
  //
  auto final_url = url_with_parameters( m_request_url, m_request_query_parameters );
  bool ok        = true;
  //
  ok = ok && easy_setopt( m_curl, CURLOPT_URL       , final_url.c_str()   ); // doesn't have to be persistent
  ok = ok && easy_setopt( m_curl, CURLOPT_WRITEDATA , &m_response_body    ); // must be persistent
  ok = ok && easy_setopt( m_curl, CURLOPT_HEADERDATA, &m_response_headers ); // must be persistent
  //
  ok = ok && fill_method();  // set m_response_code on error
  ok = ok && fill_body();    // must be called before fill_headers(), set m_response_code on error
  ok = ok && fill_headers(); // set m_curl_headers, m_response_code on error
  //
  if ( ok )
    return true;
  //
  return false;
}

//--------------------------------------------------------------------
// Called by Wrapper after a request to retrieve protocol specific details.
// Release extra curl handles that were used during the operation.
void HTTP::finalize_protocol( void )
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

//--------------------------------------------------------------------
// Called by Wrapper when the user wants to reset the Protocol.
// Clear all previous configuration before doing a new request.
void HTTP::clear_protocol( void )
{
    release_curl_extras();
    //
    m_request_method          = Method::none;
    m_request_url             .clear();
    m_request_query_parameters.clear();
    m_request_headers         .clear();
    m_request_content_type    .clear();
    m_request_body            .clear();
    m_request_body_parameters .clear();
    m_request_mime            .clear();
    //
    m_response_headers        .clear();
    m_response_content_type   .clear();
    m_response_redirect_url   .clear();
    m_response_body           .clear();
}

//--------------------------------------------------------------------
// Release extra curl handles that were used during the operation
void HTTP::release_curl_extras( void )
{
  curl_slist_free_all( m_curl_headers ); // ok on nullptr
  curl_mime_free     ( m_curl_mime    ); // ok on nullptr
  m_curl_headers = nullptr;
  m_curl_mime    = nullptr;
}

//--------------------------------------------------------------------
// Set the request method
bool HTTP::fill_method( void )
{
  const char * method = nullptr;
  //
  switch ( m_request_method )
  {
    default:
    case Method::none   : m_response_code = c_error_http_method_set;
                          return false;
    case Method::eGET   : method = "GET"   ; break;
    case Method::eDELETE: method = "DELETE"; break;
    case Method::ePOST  : method = "POST"  ; break;
    case Method::ePUT   : method = "PUT"   ; break;
    case Method::ePATCH : method = "PATCH" ; break;
  }
  //
  return easy_setopt( m_curl, CURLOPT_CUSTOMREQUEST, method ); // doesn't have to be persistent
}

//--------------------------------------------------------------------
// Add all headers to the request
bool HTTP::fill_headers( void )
{
  curl_slist_free_all( m_curl_headers ); // ok on nullptr
  m_curl_headers = nullptr;
  //
  bool ok = true;
  //
  for ( const auto & header : m_request_headers )
    ok = ok && curl_slist_checked_append( m_curl_headers, header.first + ": " + header.second );
  //
  ok = ok && easy_setopt( m_curl, CURLOPT_HTTPHEADER, m_curl_headers ); // must be persistent
  //
  if ( ! ok )
  {
    curl_slist_free_all( m_curl_headers ); // ok on nullptr
    m_curl_headers  = nullptr;
    m_response_code = c_error_http_headers_set;
  }
  //
  return ok;
}

//--------------------------------------------------------------------
// Build the body.
// Must be called before fill_headers() since it can add headers.
bool HTTP::fill_body( void )
{
  bool ok = true;
  //
  if ( m_request_method == Method::ePOST || m_request_method == Method::ePUT || m_request_method == Method::ePATCH )
  {
    if ( ! m_request_mime.empty() ) // has precedence over m_request_body and m_request_body_parameters
      return fill_body_mime();      // set m_response_code on error
    //
    if ( ! m_request_body_parameters.empty() ) // has precedence over m_request_body
    {
      m_request_content_type = "application/x-www-form-urlencoded";
      m_request_body         = encode_parameters( m_request_body_parameters );
    }
    //
    ok = ok && easy_setopt( m_curl, CURLOPT_MIMEPOST     , nullptr                );
    ok = ok && easy_setopt( m_curl, CURLOPT_POSTFIELDS   , m_request_body.c_str() ); // must be persistent
    ok = ok && easy_setopt( m_curl, CURLOPT_POSTFIELDSIZE, m_request_body.size()  ); // will add the Content-Length header
    //
    m_request_headers.insert_or_assign( "Content-Type", m_request_content_type );
  }
  else // GET, DELETE
  {
    ok = ok && easy_setopt( m_curl, CURLOPT_HTTPGET, 1L ); // disable sending the body
  }
  //
  return ok;
}

//--------------------------------------------------------------------
// Build the MIME body. MIME body can contain a list of regular parameters
// (key / value) or files. p_mime_parameters contains a vector of variants.
// Each variant represents either a parameter or a file.
bool HTTP::fill_body_mime( void )
{
  curl_mime_free( m_curl_mime );
  m_curl_mime = curl_mime_init( m_curl );
  //
  bool ok = true;
  //
  ok = ok && m_curl_mime != nullptr;
  ok = ok && m_request_mime.apply( m_curl, m_curl_mime );
  ok = ok && easy_setopt( m_curl, CURLOPT_MIMEPOST, m_curl_mime ); // must be persistent
  //
  if ( ! ok )
  {
    curl_easy_setopt( m_curl, CURLOPT_MIMEPOST, nullptr );
    //
    curl_mime_free( m_curl_mime );
    m_curl_mime     = nullptr;
    m_response_code = c_error_http_mime_set;
  }
  //
  return ok;
}

//--------------------------------------------------------------------
// Encode parameters into a application/x-www-form-urlencoded string
std::string HTTP::encode_parameters( const t_key_values & p_parameters )
{
  std::string encoded;
  //
  encoded.reserve( p_parameters.size() * c_average_parameter_length );
  //
  for ( const auto & [ key, value ] : p_parameters )
  {
    char * enc_key = curl_easy_escape( m_curl, key  .c_str(), 0 );
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

//--------------------------------------------------------------------
// Add parameters as query parameters to the given URL
std::string HTTP::url_with_parameters(
    const std::string &  p_url,
    const t_key_values & p_parameters )
{
  if ( p_parameters.empty() )
    return p_url;
  //
  std::string new_url        = p_url;
  std::string url_parameters = encode_parameters( p_parameters );
  //
  if ( size_t pos = p_url.find_last_of( '?' ); pos == std::string::npos )
    new_url += "?" + url_parameters;
  else
    new_url += "&" + url_parameters;
  //
  return new_url;
}

} // namespace curlev
