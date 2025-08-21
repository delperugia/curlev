/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#if __has_include( <rapidjson/document.h> )
  #include <rapidjson/writer.h>
  #include <rapidjson/stringbuffer.h>
#endif

#include "http.hpp"

namespace curlev
{

//--------------------------------------------------------------------
// JSON convenient functions for nlohmann/json
//--------------------------------------------------------------------

#if __has_include( <nlohmann/json.hpp> )

//--------------------------------------------------------------------
HTTP & HTTP::REST(
    const std::string &    p_url,
    const std::string &    p_verb,
    const nlohmann::json & p_json,
    const key_values &     p_query_parameters )
{
  return REQUEST( p_verb, p_url, p_query_parameters )
        .set_body( "application/json", p_json.dump() );
}

//--------------------------------------------------------------------
bool HTTP::get_json( nlohmann::json & p_json ) const noexcept
{
  if ( is_running() )
    return false;
  //
  p_json = nlohmann::json::parse( response_body(), nullptr, false ); // no exception
  //
  return ! p_json.is_discarded();
}

//--------------------------------------------------------------------
bool HTTP::response::get_json( nlohmann::json & p_json ) const noexcept
{
  p_json = nlohmann::json::parse( body, nullptr, false ); // no exception
  //
  return ! p_json.is_discarded();
}

#endif

//--------------------------------------------------------------------
// JSON convenient functions for nlohmann/json
//--------------------------------------------------------------------

#if __has_include( <rapidjson/document.h> )

namespace
{
  std::string serialize( const rapidjson::Document & p_json )
  {
    rapidjson::StringBuffer                      buffer;
    rapidjson::Writer< rapidjson::StringBuffer > writer( buffer );
    p_json.Accept( writer );
    //
    return buffer.GetString();
  }
} // namespace

//--------------------------------------------------------------------
HTTP & HTTP::REST(
    const std::string &         p_url,
    const std::string &         p_verb,
    const rapidjson::Document & p_json,
    const key_values &          p_query_parameters )
{
  return REQUEST( p_verb, p_url, p_query_parameters )
        .set_body( "application/json", serialize( p_json ) );
}

//--------------------------------------------------------------------
bool HTTP::get_json( rapidjson::Document & p_json ) const noexcept
{
  if ( is_running() )
    return false;
  //
  return ! p_json.Parse( response_body().c_str() ).HasParseError();
}

//--------------------------------------------------------------------
bool HTTP::response::get_json( rapidjson::Document & p_json ) const noexcept
{
  return ! p_json.Parse( body.c_str() ).HasParseError();
}

#endif

} // namespace curlev
