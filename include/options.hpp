/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <curl/curl.h>
#include <string>

namespace curlev
{

//--------------------------------------------------------------------
// This class is used to set various curl options from a string and
// apply them to a given curl easy handle. set can be called several
// times to set various options. The finale configuration is then
// applied when performing the request.
class Options
{
public:
  // Expect a CSKV list of options to set. Example:
  //   follow_location=1,insecure=1
  // See the reference manual for a complete description.
  // Available keys are:
  //   Name               Default  Unit          Comment
  //   accept_compression 1        0 or 1        activate compression
  //   connect_timeout    30000    milliseconds  connection timeout
  //   cookies            0        0 or 1        receive and resend cookies
  //   follow_location    0        0,1,2,3       follow HTTP 3xx redirects
  //   insecure           0        0 or 1        disables certificate validation
  //   maxredirs          5        count         maximum number of redirects allowed
  //   proxy                       string        the SOCKS or HTTP URl to a proxy
  //   rcpt_allow_fails   0        0 or 1        continue if some recipients fail
  //   timeout            30000    milliseconds  receive data timeout
  //   verbose            0        0 or 1        debug log on console
  bool set( const std::string & p_cskv );
  //
  // Apply options to curl easy handle
  bool apply( CURL * p_curl ) const;
  //
  // Reset options to their default values
  void set_default();
  //
private:
  bool        m_accept_compression = true;
  long        m_connect_timeout    = 0;
  bool        m_cookies            = false;
  long        m_follow_location    = 0;
  bool        m_insecure           = false;
  long        m_maxredirs          = 0;
  std::string m_proxy;
  bool        m_rcpt_allow_fails   = false;
  long        m_timeout            = 0;
  bool        m_verbose            = false;
};

} // namespace curlev
