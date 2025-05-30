/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <curl/curl.h>
#include <string>

namespace curlev
{

// This class is used to set credential required for a transfer.
// The finale configuration is then applied performing the request.

template < typename Protocol > class Wrapper;

class Authentication
{
public:
  // Expect a KVCS list of credential details. Example:
  //   mode=basic,user=joe,secret=abc123
  // Available keys are:
  //   Name       Comment
  //   mode       basic, digest or bearer
  //   user       for basic and digest only: user login
  //   secret     password or token
  bool set( const std::string & p_options );
  //
protected:
  template < typename Protocol > friend class Wrapper;
  //
  // Apply credential to curl easy handle
  bool apply( CURL * p_curl );
  //
  // Reset credential to their default values
  void clear( void );
  //
private:
  enum { none, basic, digest, bearer }
              m_mode = none;
  std::string m_user;
  std::string m_secret;
};

} // namespace curlev
