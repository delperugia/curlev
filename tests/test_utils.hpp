/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <string>

extern std::string c_server_httpbun;
extern std::string c_server_compress;
extern std::string c_server_certificates;

// Returns the number of attributes in the object pointed by p_path.
// Returns -1 on error.
int         json_count  ( const std::string & p_json, const std::string & p_path );

// Returns the attribute pointed by p_path in the JSON document
std::string json_extract( const std::string & p_json, const std::string & p_path );
