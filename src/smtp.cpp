/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "smtp.hpp"
#include "utils/assert_return.hpp"
#include "utils/curl_utils.hpp"
#include "utils/string_utils.hpp"

namespace curlev
{

namespace
{

  // Current date using RFC2822 format, local
  std::string date()
  {
    std::time_t     now       = std::time( nullptr );
    const std::tm * localtime = std::localtime( &now );
    //
    ASSERT_RETURN( localtime != nullptr, "" );
    //
    // NOLINTBEGIN
    char buffer[ 64 ]; /* flawfinder: ignore / buffer size is passed to strftime */
    //
    ASSERT_RETURN( std::strftime( buffer, sizeof( buffer ), "%a, %d %b %Y %H:%M:%S %z", localtime ) > 0, "" );
    //
    return buffer;
    // NOLINTEND
  }

} // namespace

//--------------------------------------------------------------------
// Accepts address with or without display name:
// - Mary Smith <mary@x.test>
// - "Mary Smith" <mary@x.test>"
// - <mary@x.test>
// - mary@x.test
smtp::address::address( std::string_view p_text )
{
  auto start   = p_text.find( '<' );
  auto end     = p_text.find( '>' );
  auto isspace = []( int c ) { return ( ::isspace( c ) != 0 || c == '"' ) ? 1 : 0; };
  //
  if ( start != std::string::npos && end != std::string::npos && end > start ) // Mary Smith <mary@x.test>
  {
    display_name = trim( p_text.substr( 0        , start           ), isspace );
    address_spec = trim( p_text.substr( start + 1, end - start - 1 ), isspace );
  }
  else // mary@x.test
  {
    address_spec = trim( p_text );
  }
}

//--------------------------------------------------------------------
void smtp::address::clear()
{
  address_spec.clear();
  display_name.clear();
}

//--------------------------------------------------------------------
// Returns the address enclosed in angle brackets, possibly with a display name
//  - "Mary Smith" <mary@x.test>
//  - <jdoe@example.org>
[[nodiscard]] std::string smtp::address::get_name_addr() const
{
  if ( display_name.empty() )
    return get_addr_spec();
  //
  return "\"" + display_name + "\" <" + address_spec + ">";
}

//--------------------------------------------------------------------
// Returns the address enclosed in angle brackets
// - <mary@x.test>
[[nodiscard]] std::string smtp::address::get_addr_spec() const
{
  return "<" + address_spec + ">";
}

//--------------------------------------------------------------------
SMTP::~SMTP()
{
  release_curl_extras();
}

//--------------------------------------------------------------------
// Send an email with MIME parts (attachments, HTML, etc.)
SMTP & SMTP::SEND(
    const std::string &      p_url,
    const smtp::address &    p_from,
    const smtp::recipients & p_to,
    const std::string &      p_subject,
    const mime::parts &      p_parts )
{
  do_if_idle(
      [ & ]()
      {
        clear(); // clear Wrapper and SMTP
        //
        bool ok = true;
        //
        // Set URL
        ok = ok && easy_setopt( m_curl, CURLOPT_URL, p_url.c_str() ); // doesn't have to be persistent
        //
        // Set MAIL FROM
        ok = ok && easy_setopt( m_curl, CURLOPT_MAIL_FROM, p_from.get_addr_spec().c_str() ); // doesn't have to be persistent
        //
        // Set RCPT TO
        ok = ok && fill_recipients( p_to ); // can set m_response_code
        //
        // Set body
        ok = ok && fill_headers( p_subject, p_from, p_to ); // set extra headers (including From and To),  can set m_response_code
        ok = ok && fill_body_mime( p_parts );               // can set m_response_code
        //
        if ( ! ok && m_response_code == c_success )
          m_response_code = c_error_url_set;
      } );
  return *this;
}

//--------------------------------------------------------------------
// Send a simple, raw, email.
// p_body should be a rfc5322 message.
SMTP & SMTP::SEND(
    const std::string &      p_url,
    const smtp::address &    p_from,
    const smtp::recipients & p_to,
          std::string &&     p_body )
{
  do_if_idle(
      [ & ]()
      {
        clear(); // clear Wrapper and SMTP
        //
        bool ok = true;
        //
        // Set URL
        ok = ok && easy_setopt( m_curl, CURLOPT_URL, p_url.c_str() ); // doesn't have to be persistent
        //
        // Set MAIL FROM
        ok = ok && easy_setopt( m_curl, CURLOPT_MAIL_FROM, p_from.get_addr_spec().c_str() ); // doesn't have to be persistent
        //
        // Set RCPT TO
        ok = ok && fill_recipients( p_to ); // can set m_response_code
        //
        // Set body
        ok = ok && fill_body( std::move( p_body ) ); // can set m_response_code
        //
        if ( ! ok && m_response_code == c_success )
          m_response_code = c_error_url_set;
      } );
  return *this;
}

//--------------------------------------------------------------------
// Add headers to the email
SMTP & SMTP::add_headers( const key_values & p_headers )
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
std::future< SMTP::response > SMTP::launch()
{
  auto promise = std::make_shared< std::promise< response > >();
  auto future  = promise->get_future();
  //
  threaded_callback( false ).start(
      [ promise ]( const SMTP & p_smtp )
      {
        response resp;
        resp.code = p_smtp.get_code();
        //
        promise->set_value( std::move( resp ) ); // NOLINT( performance-move-const-arg )
      } );
  //
  return future;
}

//--------------------------------------------------------------------
// Called by Wrapper before starting a request.
// It is guaranteed that there is no operation running.
bool SMTP::prepare_protocol()
{
  bool ok = true;
  //
  ok = ok && easy_setopt( m_curl, CURLOPT_MAIL_RCPT , m_curl_recipients ); // must be persistent
  ok = ok && easy_setopt( m_curl, CURLOPT_HTTPHEADER, m_curl_headers    ); // must be persistent
  //
  return ok;
}

//--------------------------------------------------------------------
// Called by Wrapper after a request to retrieve protocol specific details.
// Release extra curl handles that were used during the operation.
void SMTP::finalize_protocol()
{
  release_curl_extras();
}

//--------------------------------------------------------------------
// Called by Wrapper::clear() when the user wants to reset the Protocol.
// Clear all previous configuration before doing a new request.
void SMTP::clear_protocol()
{
  release_curl_extras();
}

//--------------------------------------------------------------------
// Release extra curl handles that were used during the operation
void SMTP::release_curl_extras()
{
  curl_slist_free_all( m_curl_recipients ); // ok on nullptr
  curl_slist_free_all( m_curl_headers    ); // ok on nullptr
  curl_mime_free     ( m_curl_mime       ); // ok on nullptr
  m_curl_recipients = nullptr;
  m_curl_headers    = nullptr;
  m_curl_mime       = nullptr;
}

//--------------------------------------------------------------------
// Send the SMTP recipients
bool SMTP::fill_recipients( const smtp::recipients & p_to )
{
  bool ok = true;
  //
  for ( const auto & address : p_to )
    ok = ok && curl_slist_checked_append( m_curl_recipients, address.get_addr_spec() );
  //
  if ( ! ok && m_response_code == c_success )
    m_response_code = c_error_recipients_set;
  //
  return ok;
}

//--------------------------------------------------------------------
// Send the headers of the rfc5322 body
bool SMTP::fill_headers(
    const std::string &      p_subject,
    const smtp::address &    p_from,
    const smtp::recipients & p_to )
{
  bool ok = true;
  //
  ok = ok && curl_slist_checked_append( m_curl_headers, "Date: "    + date()                 );
  ok = ok && curl_slist_checked_append( m_curl_headers, "Subject: " + p_subject              );
  ok = ok && curl_slist_checked_append( m_curl_headers, "From: "    + p_from.get_name_addr() );
  //
  for ( const auto & address : p_to )
    ok = ok && curl_slist_checked_append( m_curl_headers, "To: " + address.get_name_addr() );
  //
  if ( ! ok && m_response_code == c_success )
    m_response_code = c_error_headers_set;
  //
  return ok;
}

//--------------------------------------------------------------------
// Send the body of the rfc5322 body
bool SMTP::fill_body_mime( const mime::parts & p_parts )
{
  ASSERT_RETURN( m_curl_mime == nullptr, false );
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
  //
  return ok;
}

//--------------------------------------------------------------------
// Set the received raw body
bool SMTP::fill_body( std::string && p_body )
{
  set_request_body( std::move( p_body ) );
  //
  bool ok = true;
  //
  ok = ok && prepare_request_body(); // ASync will read from m_request_body
  //
  if ( ! ok && m_response_code == c_success )
    m_response_code = c_error_body_set;
  //
  return ok;
}

} // namespace curlev
