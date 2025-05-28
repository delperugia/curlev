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
  // Expect a KVCS list of options to set. Example:
  //   follow_location=1,insecure=1
  // Available keys are:
  //   Name              Default  Unit     Comment
  //   connect_timeout   30       second   connection timeout
  //   follow_location   0        0 or 1   follow HTTP 3xx redirects
  //   insecure          0        0 or 1   disables certificate validation
  //   timeout           30       second   receive data timeout
  //   verbose           0        0 or 1   debug log on console
  //   cookies           false    0 or 1   receive and resend cookies
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
  long m_connect_timeout = 30;    // in second
  bool m_follow_location = false; // follow HTTP 3xx redirects
  bool m_insecure        = false; // disables certificate validation
  long m_timeout         = 30;    // in second
  bool m_verbose         = false; // debug log on console
  bool m_cookies         = false; // receive and resend cookies
};

} // namespace curlev
