/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "authentication.hpp"
#include "common.hpp"

namespace curlev
{

//--------------------------------------------------------------------
// Expect a KVCS list of credential details. Example:
//   mode=basic,user=joe,secret=abc123
// Available keys are:
//   Name       Comment
//   mode       basic, digest or bearer
//   user       for basic and digest only: user login
//   secret     password or token
bool Authentication::set(const std::string& p_options)
{
  return parse_cskv(
    p_options,
    [ this ]( const std::string & key, const std::string & value )
    {
      if ( key == "mode" )
      {
               if ( value == "basic"  ) m_mode = basic;
          else if ( value == "digest" ) m_mode = digest;
          else if ( value == "bearer" ) m_mode = bearer;
          else
              return false;  // unhandled mode
      }
      else if ( key == "user"   ) m_user   = value;
      else if ( key == "secret" ) m_secret = value;
      // ignore unknown keys for forward compatibility
      //
      return true;
    } );
}

//--------------------------------------------------------------------
// Apply credential to curl easy handle.
// It returns false if any option fails to set.
// AUTH_BEARER is only fully functional starting with v7.69 (issue #5901).
bool Authentication::apply( CURL * p_curl )
{
  bool ok = true;
  //
  switch ( m_mode )
  {
  case none:
    ok = ok && easy_setopt( p_curl, CURLOPT_HTTPAUTH, CURLAUTH_NONE );
    break;
  case basic:
    ok = ok && easy_setopt( p_curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC );
    ok = ok && easy_setopt( p_curl, CURLOPT_USERNAME, m_user.c_str() );
    ok = ok && easy_setopt( p_curl, CURLOPT_PASSWORD, m_secret.c_str() );
    break;
  case digest:
    ok = ok && easy_setopt( p_curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST );
    ok = ok && easy_setopt( p_curl, CURLOPT_USERNAME, m_user.c_str() );
    ok = ok && easy_setopt( p_curl, CURLOPT_PASSWORD, m_secret.c_str() );
    break;
  case bearer:
    ok = ok && easy_setopt( p_curl, CURLOPT_HTTPAUTH      , CURLAUTH_BEARER );
    ok = ok && easy_setopt( p_curl, CURLOPT_XOAUTH2_BEARER, m_secret.c_str() );
    break;
  default:
    ok = false;
    break;
  }
  //
  return ok;
}

//--------------------------------------------------------------------
// Reset credential to its default value
void Authentication::clear( void )
{
  m_mode = none;
  m_user.clear();
  m_secret.clear();
}

} // namespace curlev
