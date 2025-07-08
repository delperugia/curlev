/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "common.hpp"
#include "options.hpp"

namespace curlev
{

// Default network timeout
constexpr auto c_timeout_ms = 30000L;

// Default maximum number of network redirect
constexpr auto c_max_redirect = 5L;

//--------------------------------------------------------------------
// Expect a CSKV list of options to set. Example:
//   follow_location=1,insecure=1
// NOLINTBEGIN(readability-misleading-indentation)
bool Options::set( const std::string & p_cskv )
{
  return parse_cskv(
    p_cskv,
    [ this ]( std::string_view key, std::string_view value )
    {
      bool ok = true;
      //
           if ( key == "accept_compression" )  m_accept_compression = ( value == "1" );
      else if ( key == "connect_timeout"    )  ok                   = svtol( value, m_connect_timeout );
      else if ( key == "cookies"            )  m_cookies            = ( value == "1" );
      else if ( key == "follow_location"    )  m_follow_location    = ( value == "1" );
      else if ( key == "insecure"           )  m_insecure           = ( value == "1" );
      else if ( key == "maxredirs"          )  ok                   = svtol( value, m_maxredirs );
      else if ( key == "proxy"              )  m_proxy              = value;
      else if ( key == "timeout"            )  ok                   = svtol( value, m_timeout );
      else if ( key == "verbose"            )  m_verbose            = ( value == "1" );
      else
          return false;  // unhandled key
      //
      return ok;
    } );
}
// NOLINTEND(readability-misleading-indentation)

//--------------------------------------------------------------------
// Apply the configured options to the given CURL easy handle.
// It returns false if any option fails to set.
bool Options::apply( CURL * p_curl ) const
{
  bool ok = true;
  //
  ok = ok && easy_setopt( p_curl, CURLOPT_ACCEPT_ENCODING  , m_accept_compression ? "" : nullptr              );
  ok = ok && easy_setopt( p_curl, CURLOPT_CONNECTTIMEOUT_MS, m_connect_timeout                                );
  ok = ok && easy_setopt( p_curl, CURLOPT_COOKIEFILE       , m_cookies            ? "" : nullptr              );
  ok = ok && easy_setopt( p_curl, CURLOPT_FOLLOWLOCATION   , m_follow_location    ? 1L : 0L                   ); // follow 30x
  ok = ok && easy_setopt( p_curl, CURLOPT_SSL_VERIFYHOST   , m_insecure           ? 0L : 2L                   );
  ok = ok && easy_setopt( p_curl, CURLOPT_SSL_VERIFYPEER   , m_insecure           ? 0L : 1L                   );
  ok = ok && easy_setopt( p_curl, CURLOPT_MAXREDIRS        , m_maxredirs );
  ok = ok && easy_setopt( p_curl, CURLOPT_PROXY            , m_proxy.empty()      ? nullptr : m_proxy.c_str() );
  ok = ok && easy_setopt( p_curl, CURLOPT_TIMEOUT_MS       , m_timeout );
  ok = ok && easy_setopt( p_curl, CURLOPT_VERBOSE          , m_verbose            ? 1L : 0L                   );
  //
  return ok;
}

//--------------------------------------------------------------------
// Reset options to their default values
void Options::set_default( void )
{
  m_accept_compression = false ;          // activate compression if true
  m_connect_timeout    = c_timeout_ms ;   // in milliseconds
  m_cookies            = false ;          // receive and resend cookies
  m_follow_location    = false ;          // follow HTTP 3xx redirects
  m_insecure           = false ;          // disables certificate validation
  m_maxredirs          = c_max_redirect;  // maximum number of redirects allowed
  m_proxy              .clear();          // the SOCKS or HTTP URl to a proxy
  m_timeout            = c_timeout_ms ;   // in milliseconds
  m_verbose            = false ;          // debug log on console
}

} // namespace curlev
