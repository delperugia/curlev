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
// When using REST functions using external JSON parsers, this function
// allows to uniformize syntax for idempotent verbs
// NOLINTBEGIN( readability-misleading-indentation )
HTTP &
  HTTP::REST( const std::string & p_uri,
              const std::string & p_verb )
{
  HTTP::Method method = Method::none;
  //
       if ( p_verb == "GET"    ) method = Method::eGET;
  else if ( p_verb == "DELETE" ) method = Method::eDELETE;
  else if ( p_verb == "POST"   ) method = Method::ePOST;
  else if ( p_verb == "PUT"    ) method = Method::ePUT;
  else if ( p_verb == "PATCH"  ) method = Method::ePATCH; // bad practice since there is no body
  //
  do_if_idle( [ & ]() {
    clear();
    //
    m_request_method = method;
    m_request_url    = p_uri;
  } );
  //
  return *this;
}
// NOLINTEND( readability-misleading-indentation )

//--------------------------------------------------------------------
// When using external JSON parser, once the JSON text is generated, prepare the query
// NOLINTBEGIN( readability-misleading-indentation )
HTTP &
    HTTP::REST( const std::string &  p_uri,
                const std::string &  p_verb,
                std::string &&       p_body )
{
  HTTP::Method method = Method::none;
  //
  // GET and DELETE can't have body
       if ( p_verb == "POST"  ) method = Method::ePOST;
  else if ( p_verb == "PUT"   ) method = Method::ePUT;
  else if ( p_verb == "PATCH" ) method = Method::ePATCH;
  //
  do_if_idle( [ & ]() {
    clear();
    //
    set_request_body( std::move( p_body ) );
    //
    m_request_method       = method;
    m_request_url          = p_uri;
    m_request_content_type = "application/json";
  } );
  //
  return *this;
}
// NOLINTEND( readability-misleading-indentation )

//--------------------------------------------------------------------
// JSON convenient functions for nlohmann/json
//--------------------------------------------------------------------

#if __has_include( <nlohmann/json.hpp> )

//--------------------------------------------------------------------
HTTP & HTTP::REST(
    const std::string &    p_uri,
    const std::string &    p_verb,
    const nlohmann::json & p_json )
{
  return REST( p_uri, p_verb, p_json.dump() );
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
    const std::string &         p_uri,
    const std::string &         p_verb,
    const rapidjson::Document & p_json )
{
  return REST( p_uri, p_verb, serialize( p_json ) );
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
