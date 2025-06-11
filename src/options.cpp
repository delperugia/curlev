/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "common.hpp"
#include "options.hpp"

namespace curlev
{

//--------------------------------------------------------------------
// Expect a CSKV list of options to set. Example:
//   follow_location=1,insecure=1
bool Options::set( const std::string & p_options )
{
  return parse_cskv(
    p_options,
    [ this ]( const std::string & key, const std::string & value )
    {
           if ( key == "accept_compression" )  m_accept_compression = ( value == "1" );
      else if ( key == "connect_timeout"    )  m_connect_timeout    = std::stoi( value );
      else if ( key == "cookies"            )  m_cookies            = ( value == "1" );
      else if ( key == "follow_location"    )  m_follow_location    = ( value == "1" );
      else if ( key == "insecure"           )  m_insecure           = ( value == "1" );
      else if ( key == "maxredirs"          )  m_maxredirs          = std::stoi( value );
      else if ( key == "proxy"              )  m_proxy              = value;
      else if ( key == "timeout"            )  m_timeout            = std::stoi( value );
      else if ( key == "verbose"            )  m_verbose            = ( value == "1" );
      // no error on unknown key to ensure forward compatibility
      //
      return true;
    } );
}

//--------------------------------------------------------------------
// Apply the configured options to the given CURL easy handle.
// It returns false if any option fails to set.
bool Options::apply( CURL * p_curl )
{
  bool ok = true;
  //
  ok = ok && easy_setopt( p_curl, CURLOPT_ACCEPT_ENCODING  , m_accept_compression ? "" : nullptr );
  ok = ok && easy_setopt( p_curl, CURLOPT_CONNECTTIMEOUT_MS, m_connect_timeout );
  ok = ok && easy_setopt( p_curl, CURLOPT_COOKIEFILE       , m_cookies ? "" : nullptr );
  ok = ok && easy_setopt( p_curl, CURLOPT_FOLLOWLOCATION   , m_follow_location ? 1L : 0L ); // follow 30x
  ok = ok && easy_setopt( p_curl, CURLOPT_SSL_VERIFYHOST   , m_insecure ? 0L : 1L );
  ok = ok && easy_setopt( p_curl, CURLOPT_SSL_VERIFYPEER   , m_insecure ? 0L : 1L );
  ok = ok && easy_setopt( p_curl, CURLOPT_MAXREDIRS        , m_maxredirs );
  ok = ok && easy_setopt( p_curl, CURLOPT_PROXY            , m_proxy.empty() ? nullptr : m_proxy.c_str() );
  ok = ok && easy_setopt( p_curl, CURLOPT_TIMEOUT_MS       , m_timeout );
  ok = ok && easy_setopt( p_curl, CURLOPT_VERBOSE          , m_verbose ? 1L : 0L );
  //
  return ok;
}

//--------------------------------------------------------------------
// Reset options to their default values
void Options::clear( void )
{
  m_accept_compression = false ;  // activate compression
  m_connect_timeout    = 30000 ;  // in milliseconds
  m_cookies            = false ;  // receive and resend cookies
  m_follow_location    = false ;  // follow HTTP 3xx redirects
  m_insecure           = false ;  // disables certificate validation
  m_maxredirs          = 5     ;  // maximum number of redirects allowed
  m_proxy              .clear();  // the SOCKS or HTTP URl to a proxy
  m_timeout            = 30000 ;  // in milliseconds
  m_verbose            = false ;  // debug log on console
}

} // namespace curlev
