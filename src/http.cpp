/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "http.hpp"
#include "utils/assert_return.hpp"
#include "utils/curl_utils.hpp"

namespace curlev
{

//--------------------------------------------------------------------
HTTP::~HTTP()
{
  release_curl_extras();
}

//--------------------------------------------------------------------
// Setup the request with the method and URL, possibly with query parameters
HTTP & HTTP::REQUEST( const std::string & p_method, std::string p_url, const key_values & p_query_parameters )
{
  do_if_idle( [ & ]() {
    clear(); // clear Wrapper and HTTP
    //
    bool already_has_parameters = p_url.rfind( '?' ) != std::string::npos;
    append_url_encoded( p_url, p_query_parameters, already_has_parameters ? '&' : '?' );
    //
    bool ok = true;
    //
    ok = ok && easy_setopt( m_curl, CURLOPT_CUSTOMREQUEST, p_method.c_str() ); // doesn't have to be persistent
    ok = ok && easy_setopt( m_curl, CURLOPT_URL          , p_url   .c_str() ); // doesn't have to be persistent
    //
    if ( ! ok && m_response_code == c_success )
      m_response_code = c_error_url_set;
  } );
  //
  return *this;
}

//--------------------------------------------------------------------
// Add headers to the request
HTTP & HTTP::add_headers( const key_values & p_headers )
{
  do_if_idle( [ & ]() {
    bool ok = true;
    //
    for ( const auto & header : p_headers )
      ok = ok && curl_slist_checked_append( m_curl_headers, header.first + ": " + header.second );
    //
    if ( ! ok && m_response_code == c_success )
      m_response_code = c_error_headers_set;
  } );
  //
  return *this;
}

//--------------------------------------------------------------------
// Set a raw body
HTTP & HTTP::set_body( const std::string & p_content_type, std::string && p_body )
{
  do_if_idle( [ & ]() {
    ASSERT_RETURN_VOID( m_curl_mime == nullptr && request_body().empty() ); // avoid setting body twice
    //
    set_request_body( std::move( p_body ) );
    //
    bool ok = true;
    //
    ok = ok && curl_slist_checked_append( m_curl_headers, "Content-Type: " + p_content_type );
    ok = ok && prepare_request_body(); // ASync will read from m_request_body
    //
    if ( ! ok && m_response_code == c_success )
      m_response_code = c_error_body_set;
  } );
  //
  return *this;
}

//--------------------------------------------------------------------
// Set body parameters to the request, URL encoded
HTTP & HTTP::set_parameters( const key_values & p_body_parameters )
{
  std::string body;
  append_url_encoded( body, p_body_parameters );
  //
  return set_body( "application/x-www-form-urlencoded", std::move( body ) );
}

//--------------------------------------------------------------------
// Set MIME parts to the body of the request
HTTP & HTTP::set_mime( const mime::parts & p_parts )
{
  do_if_idle( [ & ]() {
    ASSERT_RETURN_VOID( m_curl_mime == nullptr && request_body().empty() ); // avoid setting body twice
    //
    m_curl_mime = curl_mime_init( m_curl );
    //
    bool ok = true;
    //
    ok = ok && m_curl_mime != nullptr;
    ok = ok && mime::apply( m_curl, m_curl_mime, p_parts );
    ok = ok && easy_setopt( m_curl, CURLOPT_MIMEPOST, m_curl_mime ); // must be persistent
    //
    if ( ! ok && m_response_code == c_success )
      m_response_code = c_error_mime_set;
  } );
  //
  return *this;
}

//--------------------------------------------------------------------
// These accessors return references for efficiency, directly exposing the object's
// internal. They must only be used after the request has fully completed to ensure data
// consistency and avoid unstable values during ongoing operations.
static const std::string   c_empty_string;
static const key_values_ci c_empty_key_values_ci;

const key_values_ci & HTTP::get_headers     () const noexcept { return is_running() ? c_empty_key_values_ci : response_headers();      }
const std::string   & HTTP::get_body        () const noexcept { return is_running() ? c_empty_string        : response_body();         }
const std::string   & HTTP::get_content_type() const noexcept { return is_running() ? c_empty_string        : m_response_content_type; }
const std::string   & HTTP::get_redirect_url() const noexcept { return is_running() ? c_empty_string        : m_response_redirect_url; }

//--------------------------------------------------------------------
std::future< HTTP::response > HTTP::launch()
{
  auto promise = std::make_shared< std::promise< HTTP::response > >();
  auto future  = promise->get_future();
  //
  threaded_callback( false ). // because the callback in start() is fast
  start(
      [ promise ]( const HTTP & p_http )
      {
        // Tricky part: we are certain of the mutability of HTTP, the transfer
        // is guaranteed to be finished, join() is not yet released. state is
        // either idle or finished.
        // The original reason to have a const is to prevent the end user from
        // restarting a request from with the callback. It is not a concern here.
        // (see Wrapper::cb_protocol)
        auto & http = const_cast< HTTP & >( p_http ); // NOLINT( cppcoreguidelines-pro-type-const-cast )
        //
        response resp;
        http.take_response_headers( resp.headers );
        http.take_response_body   ( resp.body    );
        resp.code         = http.get_code();
        resp.content_type = std::move( http.m_response_content_type );
        resp.redirect_url = std::move( http.m_response_redirect_url );
        //
        promise->set_value( std::move( resp ) );
      } );
  //
  return future;
}

//--------------------------------------------------------------------
// Called by Wrapper before starting a request.
// It is guaranteed that there is no operation running.
bool HTTP::prepare_protocol()
{
  bool ok = true;
  //
  ok = ok && curl_slist_checked_append( m_curl_headers, "Expect: " );   // to prevent libcurl to send Expect
  ok = ok && easy_setopt( m_curl, CURLOPT_HTTPHEADER, m_curl_headers ); // must be persistent
  //
  return ok;
}

//--------------------------------------------------------------------
// Called by Wrapper after a request to retrieve protocol specific details.
// Release extra curl handles that were used during the operation.
void HTTP::finalize_protocol()
{
  char * value = nullptr;
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
// Called by Wrapper::clear() when the user wants to reset the Protocol.
// Clear all previous configuration before doing a new request.
void HTTP::clear_protocol()
{
    release_curl_extras();
    //
    m_response_content_type.clear();
    m_response_redirect_url.clear();
}

//--------------------------------------------------------------------
// Release extra curl handles that were used during the operation
void HTTP::release_curl_extras()
{
  curl_slist_free_all( m_curl_headers ); // ok on nullptr
  curl_mime_free     ( m_curl_mime    ); // ok on nullptr
  m_curl_headers = nullptr;
  m_curl_mime    = nullptr;
}

} // namespace curlev
