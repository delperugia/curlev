/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "certificates.hpp"
#include "common.hpp"

namespace curlev
{

//--------------------------------------------------------------------
// Expect a CSKV list of credential details. Example:
//   sslcert=client.pem,sslkey=key.pem,keypasswd=s3cret
bool Certificates::set( const std::string & p_options )
{
  return parse_cskv(
    p_options,
    [ this ]( std::string_view key, std::string_view value )
    {
      // no error on unknown key to ensure forward compatibility
      m_parameters[ std::string( key ) ] = value;
      return true;
    } );
}

//--------------------------------------------------------------------
// Apply credential to curl easy handle.
// It returns false if any option fails to set.
bool Certificates::apply( CURL * p_curl )
{
  bool ok = true;
  //
  ok = ok && setopt   ( p_curl, "engine"            , CURLOPT_SSLENGINE         );
  ok = ok && setopt   ( p_curl, "sslcert"           , CURLOPT_SSLCERT           );
  ok = ok && setopt   ( p_curl, "sslcerttype"       , CURLOPT_SSLCERTTYPE       );
  ok = ok && setopt   ( p_curl, "sslkey"            , CURLOPT_SSLKEY            );
  ok = ok && setopt   ( p_curl, "sslkeytype"        , CURLOPT_SSLKEYTYPE        );
  ok = ok && setopt   ( p_curl, "keypasswd"         , CURLOPT_KEYPASSWD         );
  ok = ok && setopt_ca( p_curl, "cainfo"            , CURLOPT_CAINFO            , m_ca_info );
  ok = ok && setopt_ca( p_curl, "capath"            , CURLOPT_CAPATH            , m_ca_path );
  ok = ok && setopt   ( p_curl, "proxy_sslcert"     , CURLOPT_PROXY_SSLCERT     );
  ok = ok && setopt   ( p_curl, "proxy_sslcerttype" , CURLOPT_PROXY_SSLCERTTYPE );
  ok = ok && setopt   ( p_curl, "proxy_sslkey"      , CURLOPT_PROXY_SSLKEY      );
  ok = ok && setopt   ( p_curl, "proxy_sslkeytype"  , CURLOPT_PROXY_SSLKEYTYPE  );
  ok = ok && setopt   ( p_curl, "proxy_keypasswd"   , CURLOPT_PROXY_KEYPASSWD   );
  ok = ok && setopt_ca( p_curl, "proxy_cainfo"      , CURLOPT_PROXY_CAINFO      , m_ca_info );
  ok = ok && setopt_ca( p_curl, "proxy_capath"      , CURLOPT_PROXY_CAPATH      , m_ca_path );
  //
  return ok;
}

//--------------------------------------------------------------------
// Reset credential to its default value
// libcurl doesn't reset the CA keys to their default values when setting nullptr,
// so we always restore the values captured during startup (by ASync).
// Before libcurl 7.84.0 it was not possible to retrieve them. 
void Certificates::set_default( const std::string & p_ca_info, const std::string & p_ca_path )
{
  m_parameters.clear();
  //
  m_ca_info = p_ca_info;
  m_ca_path = p_ca_path;
}

//--------------------------------------------------------------------
// Set a standard option. If not set by the user, or set to empty, reset to the
// default value by using nullptr.
bool Certificates::setopt( CURL * p_curl, const char * p_key, CURLoption p_option )
{
  if ( const auto iter = m_parameters.find( p_key );
       iter == m_parameters.end() || iter->second.empty() )       // not configured or set to empty
    return easy_setopt( p_curl, p_option, nullptr );              // set default
  else
    return easy_setopt( p_curl, p_option, iter->second.c_str() ); // set the value
}

//--------------------------------------------------------------------
// For CA it is not possible to reset to default. Either the default is passed
// and is used, or (for libcurl <7.84.0) nothing is done.
bool Certificates::setopt_ca( CURL * p_curl, const char * p_key, CURLoption p_option, const std::string & p_default )
{
  if ( const auto iter = m_parameters.find( p_key );
       iter == m_parameters.end() || iter->second.empty() )       // not configured or set to empty
  {
    if ( p_default.empty() )
      return true;                                                // do nothing
    else
      return easy_setopt( p_curl, p_option, p_default.c_str() );  // reset known default
  }
  else
    return easy_setopt( p_curl, p_option, iter->second.c_str() ); // set the value
}

} // namespace curlev
