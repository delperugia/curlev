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
// This class is used to set credential required for a transfer.
// The finale configuration is then applied when performing the request.
class Authentication
{
public:
  // Expect a CSKV list of credential details. Example:
  //   mode=basic,user=joe,secret=abc123
  // See the reference manual for a complete description.
  // Available keys are:
  //   Name       Comment
  //   mode       basic, digest or bearer
  //   user       for basic and digest only: user login
  //   secret     password or token
  bool set( const std::string & p_cskv );
  //
  // Apply credential to curl easy handle
  bool apply( CURL * p_curl ) const;
  //
  // Reset credential to their default values
  void set_default();
  //
private:
  enum class Mode { none, basic, digest, bearer };
  //
  Mode        m_mode = Mode::none;
  std::string m_user;
  std::string m_secret;
};

} // namespace curlev
