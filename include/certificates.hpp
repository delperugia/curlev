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
  // Expect a CSKV list of todo
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
};

} // namespace curlev
