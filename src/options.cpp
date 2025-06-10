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
// Available keys are:
//   Name              Default  Unit          Comment
//   connect_timeout   30000    milliseconds  connection timeout
//   cookies           false    0 or 1        receive and resend cookies
//   follow_location   false    0 or 1        follow HTTP 3xx redirects
//   insecure          false    0 or 1        disables certificate validation
//   maxredirs         5        count         maximum number of redirects allowed
//   proxy                      string        the SOCKS or HTTP URl to a proxy
//   timeout           30000    milliseconds  receive data timeout
//   verbose           false    0 or 1        debug log on console
bool Options::set( const std::string & p_options )
{
  return parse_cskv(
    p_options,
    [ this ]( const std::string & key, const std::string & value )
    {
           if ( key == "connect_timeout" )  m_connect_timeout = std::stoi( value );
      else if ( key == "cookies"         )  m_cookies         = ( value == "1" );
      else if ( key == "follow_location" )  m_follow_location = ( value == "1" );
      else if ( key == "insecure"        )  m_insecure        = ( value == "1" );
      else if ( key == "maxredirs"       )  m_maxredirs       = std::stoi( value );
      else if ( key == "proxy"           )  m_proxy           = value;
      else if ( key == "timeout"         )  m_timeout         = std::stoi( value );
      else if ( key == "verbose"         )  m_verbose         = ( value == "1" );
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
  m_connect_timeout = Options().m_connect_timeout;
  m_cookies         = Options().m_cookies;
  m_follow_location = Options().m_follow_location;
  m_insecure        = Options().m_insecure;
  m_maxredirs       = Options().m_maxredirs;
  m_proxy           = Options().m_proxy;
  m_timeout         = Options().m_timeout;
  m_verbose         = Options().m_verbose;
}

} // namespace curlev
