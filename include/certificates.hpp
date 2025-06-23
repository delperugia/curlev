/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <curl/curl.h>
#include <map>
#include <string>

namespace curlev
{

//--------------------------------------------------------------------
// This class is used to set certificates for direct and proxy connections.
// The finale configuration is then applied when performing the request.
class Certificates
{
public:
  // Expect a CSKV list of certificate parameters. Example:
  //   sslcert=client.pem,sslkey=key.pem,keypasswd=s3cret
  // See the reference manual for a complete description.
  bool set( const std::string & p_options );
  //
  // Apply credential to curl easy handle
  bool apply( CURL * p_curl ) const;
  //
  // Reset credential to their default values
  void set_default( const std::string & p_ca_info, const std::string & p_ca_path );
  //
private:
  std::string m_engine;
  std::string m_sslcert;
  std::string m_sslcerttype;
  std::string m_sslkey;
  std::string m_sslkeytype;
  std::string m_keypasswd;
  std::string m_cainfo;
  std::string m_capath;
  std::string m_proxy_sslcert;
  std::string m_proxy_sslcerttype;
  std::string m_proxy_sslkey;
  std::string m_proxy_sslkeytype;
  std::string m_proxy_keypasswd;
  std::string m_proxy_cainfo;
  std::string m_proxy_capath;
  //
  std::string m_ca_info_default; // default CURLINFO_CAINFO
  std::string m_ca_path_default; // default CURLINFO_CAPATH
};

} // namespace curlev
