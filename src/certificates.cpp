/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "certificates.hpp"
#include "utils/string_utils.hpp"
#include "utils/curl_utils.hpp"

namespace curlev
{

//--------------------------------------------------------------------
// Expect a CSKV list of credential details. Example:
//   sslcert=client.pem,sslkey=key.pem,keypasswd=s3cret
// if-else is twice faster than unordered_map in almost all cases.
// NOLINTBEGIN(readability-misleading-indentation)
bool Certificates::set( const std::string & p_cskv )
{
  return parse_cskv(
    p_cskv,
    [ this ]( std::string_view key, std::string_view value )
    {
           if ( key == "sslcert"           ) m_sslcert           = value;
      else if ( key == "sslcerttype"       ) m_sslcerttype       = value;
      else if ( key == "sslkey"            ) m_sslkey            = value;
      else if ( key == "sslkeytype"        ) m_sslkeytype        = value;
      else if ( key == "keypasswd"         ) m_keypasswd         = value;
      else if ( key == "cainfo"            ) m_cainfo            = value;
      else if ( key == "capath"            ) m_capath            = value;
      else if ( key == "proxy_sslcert"     ) m_proxy_sslcert     = value;
      else if ( key == "proxy_sslcerttype" ) m_proxy_sslcerttype = value;
      else if ( key == "proxy_sslkey"      ) m_proxy_sslkey      = value;
      else if ( key == "proxy_sslkeytype"  ) m_proxy_sslkeytype  = value;
      else if ( key == "proxy_keypasswd"   ) m_proxy_keypasswd   = value;
      else if ( key == "proxy_cainfo"      ) m_proxy_cainfo      = value;
      else if ( key == "proxy_capath"      ) m_proxy_capath      = value;
      else if ( key == "engine"            ) m_engine            = value;
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
namespace
{
  // If a parameter is not set, reset its value to the default by using nullptr
  bool easy_setopt_std( CURL * p_curl, CURLoption p_option, const std::string & p_value )
  {
    return easy_setopt(
      p_curl,
      p_option,
      p_value.empty() ?    // if set to empty
        nullptr :          //   reset to default
        p_value.c_str() ); //   else set the value
  }

  // For CA it is not possible to reset to default. Either the default is passed
  // and is used, or (for libcurl <7.84.0) nothing is done.
  bool easy_setopt_ca(
      CURL *              p_curl,
      CURLoption          p_option,
      const std::string & p_value,
      const std::string & p_default )
  {
    if ( p_value.empty() ) // try to reset default
    {
      if ( p_default.empty() )
        return true; // do nothing
      //
      return easy_setopt( p_curl, p_option, p_default.c_str() ); // reset known default
    }
    //
    return easy_setopt( p_curl, p_option, p_value.c_str() ); // set the value
  }
} // namespace

bool Certificates::apply( CURL * p_curl ) const
{
  bool ok = true;
  //
  ok = ok && easy_setopt_std( p_curl, CURLOPT_SSLENGINE        , m_engine                          );
  ok = ok && easy_setopt_std( p_curl, CURLOPT_SSLCERT          , m_sslcert                         );
  ok = ok && easy_setopt_std( p_curl, CURLOPT_SSLCERTTYPE      , m_sslcerttype                     );
  ok = ok && easy_setopt_std( p_curl, CURLOPT_SSLKEY           , m_sslkey                          );
  ok = ok && easy_setopt_std( p_curl, CURLOPT_SSLKEYTYPE       , m_sslkeytype                      );
  ok = ok && easy_setopt_std( p_curl, CURLOPT_KEYPASSWD        , m_keypasswd                       );
  ok = ok && easy_setopt_ca ( p_curl, CURLOPT_CAINFO           , m_cainfo      , m_ca_info_default );
  ok = ok && easy_setopt_ca ( p_curl, CURLOPT_CAPATH           , m_capath      , m_ca_path_default );
  //
  ok = ok && easy_setopt_std( p_curl, CURLOPT_PROXY_SSLCERT    , m_proxy_sslcert                   );
  ok = ok && easy_setopt_std( p_curl, CURLOPT_PROXY_SSLCERTTYPE, m_proxy_sslcerttype               );
  ok = ok && easy_setopt_std( p_curl, CURLOPT_PROXY_SSLKEY     , m_proxy_sslkey                    );
  ok = ok && easy_setopt_std( p_curl, CURLOPT_PROXY_SSLKEYTYPE , m_proxy_sslkeytype                );
  ok = ok && easy_setopt_std( p_curl, CURLOPT_PROXY_KEYPASSWD  , m_proxy_keypasswd                 );
  ok = ok && easy_setopt_ca ( p_curl, CURLOPT_PROXY_CAINFO     , m_proxy_cainfo, m_ca_info_default );
  ok = ok && easy_setopt_ca ( p_curl, CURLOPT_PROXY_CAPATH     , m_proxy_capath, m_ca_path_default );
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
  m_engine           .clear();
  m_sslcert          .clear();
  m_sslcerttype      .clear();
  m_sslkey           .clear();
  m_sslkeytype       .clear();
  m_keypasswd        .clear();
  m_cainfo           .clear();
  m_capath           .clear();
  m_proxy_sslcert    .clear();
  m_proxy_sslcerttype.clear();
  m_proxy_sslkey     .clear();
  m_proxy_sslkeytype .clear();
  m_proxy_keypasswd  .clear();
  m_proxy_cainfo     .clear();
  m_proxy_capath     .clear();
  //
  m_ca_info_default = p_ca_info;
  m_ca_path_default = p_ca_path;
}

// feat(erase_memory_secrets): m_keypasswd, m_proxy_keypasswd, CURLOPT_KEYPASSWD, CURLOPT_PROXY_KEYPASSWD

} // namespace curlev
