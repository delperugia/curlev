/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "authentication.hpp"
#include "common.hpp"

namespace curlev
{

//--------------------------------------------------------------------
// Expect a CSKV list of credential details. Example:
//   mode=basic,user=joe,secret=abc123
// NOLINTBEGIN(readability-misleading-indentation)
bool Authentication::set( const std::string& p_cskv )
{
  return parse_cskv(
    p_cskv,
    [ this ]( std::string_view key, std::string_view value )
    {
      if ( key == "mode" )
      {
               if ( value == "bearer" ) m_mode = Mode::bearer;
          else if ( value == "digest" ) m_mode = Mode::digest;
          else if ( value == "basic"  ) m_mode = Mode::basic;
          else if ( value == "none"   ) m_mode = Mode::none;
          else
              return false;  // unhandled mode
      }
      else if ( key == "user"   ) m_user   = value;
      else if ( key == "secret" ) m_secret = value;
      else
          return false;  // unhandled key
      //
      return true;
    } );
}
// NOLINTEND(readability-misleading-indentation)

//--------------------------------------------------------------------
// Apply credential to curl easy handle.
// It returns false if any option fails to set.
// AUTH_BEARER:
//  - is only fully functional starting with v7.69 (issue #5901).
//  - has memory leak with v<7.84 (issues #8841)
bool Authentication::apply( CURL * p_curl ) const
{
  bool ok = true;
  //
  switch ( m_mode )
  {
  case Mode::none:
    ok = ok && easy_setopt( p_curl, CURLOPT_HTTPAUTH, CURLAUTH_NONE );
    break;
  case Mode::basic:
    ok = ok && easy_setopt( p_curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC );
    ok = ok && easy_setopt( p_curl, CURLOPT_USERNAME, m_user.c_str() );
    ok = ok && easy_setopt( p_curl, CURLOPT_PASSWORD, m_secret.c_str() );
    break;
  case Mode::digest:
    ok = ok && easy_setopt( p_curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST );
    ok = ok && easy_setopt( p_curl, CURLOPT_USERNAME, m_user.c_str() );
    ok = ok && easy_setopt( p_curl, CURLOPT_PASSWORD, m_secret.c_str() );
    break;
  case Mode::bearer:
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
void Authentication::set_default()
{
  m_mode  = Mode::none; // no authentication
  m_user  .clear();     // user login
  m_secret.clear();     // user password or access token
}

} // namespace curlev
