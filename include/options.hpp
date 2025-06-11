/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <curl/curl.h>
#include <string>

namespace curlev
{

// This class is used to set various curl options from a string and
// apply them to a given curl easy handle. set can be called several
// times to set various options. The finale configuration is then
// applied performing the request.

template < typename Protocol > class Wrapper;

class Options
{
public:
  Options() { clear() ; }
  //
  // Expect a KVCS list of options to set. Example:
  //   follow_location=1,insecure=1
  // Available keys are:
  //   Name               Default  Unit          Comment
  //   accept_compression 0        0 or 1        activate compression
  //   connect_timeout    30000    milliseconds  connection timeout
  //   cookies            false    0 or 1        receive and resend cookies
  //   follow_location    false    0 or 1        follow HTTP 3xx redirects
  //   insecure           false    0 or 1        disables certificate validation
  //   maxredirs          5        count         maximum number of redirects allowed
  //   proxy                       string        the SOCKS or HTTP URl to a proxy
  //   timeout            30000    milliseconds  receive data timeout
  //   verbose            false    0 or 1        debug log on console
  bool set( const std::string & p_options );
  //
protected:
  template < typename Protocol > friend class Wrapper;
  //
  // Apply options to curl easy handle
  bool apply( CURL * p_curl );
  //
  // Reset options to their default values
  void clear( void );
  //
private:
  bool        m_accept_compression;
  long        m_connect_timeout;
  bool        m_cookies;
  bool        m_follow_location;
  bool        m_insecure;
  long        m_maxredirs;
  std::string m_proxy;
  long        m_timeout;
  bool        m_verbose;
};

} // namespace curlev
