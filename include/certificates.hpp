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
  bool apply( CURL * p_curl );
  //
  // Reset credential to their default values
  void set_default( const std::string & p_ca_info, const std::string & p_ca_path );
  //
private:
  std::map< std::string, std::string > m_parameters; // contains all configured keys
  //
  std::string m_ca_info; // default CURLINFO_CAINFO
  std::string m_ca_path; // default CURLINFO_CAPATH
  //
  // Set individual options
  bool setopt   ( CURL * p_curl, const char * p_key, CURLoption p_option );
  bool setopt_ca( CURL * p_curl, const char * p_key, CURLoption p_option, const std::string & p_default );
};

} // namespace curlev
