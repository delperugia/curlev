/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

std::string c_server_httpbun      = "http://httpbun.com:80/";
std::string c_server_compress     = "https://github.com/delperugia/curlev/blob/master/README.md";
std::string c_server_certificates = "https://github.com/delperugia/curlev/blob/master/.gitignore";

//--------------------------------------------------------------------
// Returns the number of attributes in the object pointed by p_path.
// Returns -1 on error.
int json_count( const std::string & p_json, const std::string & p_path )
{
  try
  {
    nlohmann::json json = nlohmann::json::parse( p_json );
    //
    // Remove leading "$." from path
    std::string clean_path = p_path.substr( 2 );
    //
    // Split path into components
    std::istringstream           path_stream( clean_path );
    std::string                  component;
    nlohmann::json::json_pointer ptr;
    //
    while ( std::getline( path_stream, component, '.' ) )
      ptr /= component;
    //
    auto element = json[ ptr ];
    return element.is_object() ? element.size() : 0;
  }
  catch ( const std::exception & e )
  {
    GTEST_LOG_(INFO) << "json_count: " << p_path << " " << e.what() << "\n" << p_json << "\n";
    return -1;
  }
}

//--------------------------------------------------------------------
// Returns the attribute pointed by p_path in the JSON document
std::string json_extract( const std::string & p_json, const std::string & p_path )
{
  nlohmann::json json = nlohmann::json::parse( p_json );
  //
  // Remove leading "$." from path
  std::string clean_path = p_path.substr( 2 );

  // Split path into components
  std::istringstream           path_stream( clean_path );
  nlohmann::json::json_pointer ptr;
  //
  try
  {
    std::string component;
    //
    while ( std::getline( path_stream, component, '.' ) )
      ptr /= component;
    //
    return json[ ptr ].get< std::string >();
  }
  catch ( const std::exception & e )
  {
    GTEST_LOG_(INFO) << "json_extract: " << p_path << " " << e.what() << "\n" << p_json << "\n";
    return "?";
  }
}
