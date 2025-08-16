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

  // Current date using RFC2822 format, UTC
  std::string date()
  {
    std::time_t     now    = std::time( nullptr );
    const std::tm * gmtime = std::gmtime( &now );
    //
    // NOLINTBEGIN
    char buffer[ 64 ];
    std::strftime( buffer, sizeof( buffer ), "%a, %d %b %Y %H:%M:%S +0000", gmtime );
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
        m_request_url     = p_url;
        m_request_from    = p_from;
        m_request_to      = p_to;
        m_request_subject = p_subject;
        m_request_mime.add_parts( p_parts );
        m_request_body.clear();
      } );
  return *this;
}

//--------------------------------------------------------------------
// Send a simple, raw, email
// p_body should be a rfc5322 message
SMTP & SMTP::SEND(
    const std::string &      p_url,
    const smtp::address &    p_from,
    const smtp::recipients & p_to,
    const std::string &      p_body )
{
  do_if_idle(
      [ & ]()
      {
        clear(); // clear Wrapper and SMTP
        //
        m_request_url     = p_url;
        m_request_from    = p_from;
        m_request_to      = p_to;
        m_request_body    = p_body;
        m_request_mime.clear();
      } );
  return *this;
}

//--------------------------------------------------------------------
// Add headers to the email
SMTP & SMTP::add_headers( const key_values & p_headers )
{
  do_if_idle( [ & ]() {
    m_request_headers.insert( p_headers.begin(), p_headers.end() );
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
  // Set URL
  ok = ok && easy_setopt( m_curl, CURLOPT_URL, m_request_url.c_str() );
  //
  // Set MAIL FROM
  ok = ok && easy_setopt( m_curl, CURLOPT_MAIL_FROM, m_request_from.get_addr_spec().c_str() ); // doesn't have to be persistent
  //
  // Set RCPT TO
  ok = ok && fill_recipients();
  //
  // Set body or MIME
  if ( m_request_mime.empty() )
  {
    ok = ok && fill_body();
  }
  else
  {
    ok = ok && fill_headers(); // set extra headers (including From and To)
    ok = ok && fill_body_mime();
  }
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
  //
  m_request_url    .clear();
  m_request_from   .clear();
  m_request_to     .clear();
  m_request_subject.clear();
  m_request_headers.clear();
  m_request_body   .clear();
  m_request_mime   .clear();
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
bool SMTP::fill_recipients()
{
  curl_slist_free_all( m_curl_recipients ); // ok on nullptr
  m_curl_recipients = nullptr;
  //
  bool ok = true;
  //
  for ( const auto & address : m_request_to )
    ok = ok && curl_slist_checked_append( m_curl_recipients, address.get_addr_spec() );
  //
  ok = ok && easy_setopt( m_curl, CURLOPT_MAIL_RCPT, m_curl_recipients ); // must be persistent
  //
  if ( ! ok )
  {
    curl_slist_free_all( m_curl_recipients ); // ok on nullptr
    m_curl_recipients  = nullptr;
    m_response_code = c_error_smtp_recipient_set;
  }
  //
  return ok;
}

//--------------------------------------------------------------------
// Send the headers of the rfc5322 body
bool SMTP::fill_headers()
{
  // Start by adding system headers
  //
  m_request_headers.insert_or_assign( "Date"   , date()                         );
  m_request_headers.insert_or_assign( "Subject", m_request_subject              );
  m_request_headers.insert_or_assign( "From"   , m_request_from.get_name_addr() );
  //
  for ( const auto & address : m_request_to )
    m_request_headers.insert_or_assign( "To", address.get_name_addr() );
  //
  // Then pass them to curl
  //
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
    m_response_code = c_error_headers_set;
  }
  //
  return ok;
}

//--------------------------------------------------------------------
// Send the body of the rfc5322 body
bool SMTP::fill_body_mime()
{
  curl_mime_free( m_curl_mime ); // ok on nullptr
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
    curl_mime_free( m_curl_mime ); // ok on nullptr
    m_curl_mime     = nullptr;
    m_response_code = c_error_mime_set;
  }
  //
  return ok;
}

//--------------------------------------------------------------------
// Send the rfc5322 message (headers + body)
bool SMTP::fill_body()
{
  return prepare_request_body(); // ASync will read from m_request_body
}

} // namespace curlev
