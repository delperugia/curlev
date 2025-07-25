/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <cstddef>
#include <curl/curl.h>
#include <future>
#include <string>

#if __has_include( <nlohmann/json.hpp> )
  #include <nlohmann/json.hpp>
#endif
#if __has_include( <rapidjson/document.h> )
  #include <rapidjson/document.h>
#endif

#include "mime.hpp"
#include "wrapper.hpp"

namespace curlev
{

//--------------------------------------------------------------------
// A specialization of the Wrapper for the HTTP protocol.
// It supports POST, PUT, PATCH, GET and DELETE methods, with parameters in query.
// For POST, PUT, PATCH a body can be specified using one of:
//  - a raw content type and body
//  - encoded parameters (content type application/x-www-form-urlencoded)
//  - a MIME document of parameters and documents
// Headers, parameters can be added if needed.
class HTTP : public Wrapper< HTTP >
{
public:
  ~HTTP() override;
  //
  // The first step is to call one of theses method:
  //
  // Simple GET and DELETE, with optional query parameters
  HTTP & GET   ( const std::string & p_url, const t_key_values & p_query_parameters = {} );
  HTTP & DELETE( const std::string & p_url, const t_key_values & p_query_parameters = {} );
  //
  // [1] Empty requests
  HTTP & PATCH( const std::string & p_url ) { return PATCH( p_url, "", "" ); }
  HTTP & POST ( const std::string & p_url ) { return POST ( p_url, "", "" ); }
  HTTP & PUT  ( const std::string & p_url ) { return PUT  ( p_url, "", "" ); }
  //
  // [2] Requests with a raw body
  HTTP & PATCH( const std::string & p_url, const std::string & p_content_type, const std::string & p_body );
  HTTP & POST ( const std::string & p_url, const std::string & p_content_type, const std::string & p_body );
  HTTP & PUT  ( const std::string & p_url, const std::string & p_content_type, const std::string & p_body );
  //
  // [3] Requests with a body made of url encoded parameters
  HTTP & PATCH( const std::string & p_url, const t_key_values & p_body_parameter ) { return PATCH( p_url ).add_body_parameters( p_body_parameter ); }
  HTTP & POST ( const std::string & p_url, const t_key_values & p_body_parameter ) { return POST ( p_url ).add_body_parameters( p_body_parameter ); }
  HTTP & PUT  ( const std::string & p_url, const t_key_values & p_body_parameter ) { return PUT  ( p_url ).add_body_parameters( p_body_parameter ); }
  //
  // Working with REST (JSON PATCH, POST and PUT requests)
  #if __has_include( <nlohmann/json.hpp> )
    HTTP & REST( const std::string & p_uri, const std::string & p_verb, const nlohmann::json & p_json );
  #endif
  #if __has_include( <rapidjson/document.h> )
    HTTP & REST( const std::string & p_uri, const std::string & p_verb, const rapidjson::Document & p_json );
  #endif
  //
  // To uniformize syntax with idempotent verbs (GET, DELETE)
  HTTP & REST( const std::string & p_uri, const std::string & p_verb );
  //
  // Add headers to the request
  HTTP & add_headers( const t_key_values & p_headers );
  //
  // Add query parameters to the request
  HTTP & add_query_parameters( const t_key_values & p_query_parameters );
  //
  // Add body parameters to the request. Only for requests [1] and [3]
  HTTP & add_body_parameters( const t_key_values & p_body_parameter );
  //
  // Add MIME part or parts. Only for requests [1]
  HTTP & add_mime_parameters( const mime::parts & p_parts );
  //
  // These accessors return references for efficiency, directly exposing the object's
  // internal. They must only be used after the request has fully completed to ensure data
  // consistency and avoid unstable values during ongoing operations.
  const t_key_values_ci & get_headers     () const noexcept;
  const std::string     & get_content_type() const noexcept;
  const std::string     & get_redirect_url() const noexcept;
  const std::string     & get_body        () const noexcept;
  //
  #if __has_include( <nlohmann/json.hpp> )
    bool get_json( nlohmann::json & p_json ) const noexcept;
  #endif
  #if __has_include( <rapidjson/document.h> )
    bool get_json( rapidjson::Document & p_json ) const noexcept;
  #endif
  //
  // Convenient function starting the request and returning a future.
  // If using launch(), the Wrapper functions start() and join() must not
  // be used.
  struct Response
  {
    long            code;
    t_key_values_ci headers;
    std::string     redirect_url;
    std::string     content_type;
    std::string     body;
    //
    #if __has_include( <nlohmann/json.hpp> )
      bool get_json( nlohmann::json & p_json ) const noexcept;
    #endif
    #if __has_include( <rapidjson/document.h> )
      bool get_json( rapidjson::Document & p_json ) const noexcept;
    #endif
  };
  //
  std::future< Response > launch();
  //
protected:
  // Prevent creating directly an instance of the class, the Wrapper::create() method must be used
  explicit HTTP( ASync & p_async ) : Wrapper< HTTP >( p_async ) {};
  //
  // Called by Wrapper before starting a request
  bool prepare_protocol() override;
  //
  // Called by Wrapper after a request completes to retrieve protocol specific details
  void finalize_protocol() override;
  //
  // Called by Wrapper when the user wants to reset the Protocol
  void clear_protocol() override;
  //
private:
  // Data used when sending the request
  enum class Method { none, eGET, eDELETE, ePOST, ePUT, ePATCH };
  //
  Method        m_request_method = Method::none;
  std::string   m_request_url;
  t_key_values  m_request_query_parameters;
  t_key_values  m_request_headers;
  std::string   m_request_content_type;
  std::string   m_request_body;            // must be persistent (CURLOPT_POSTFIELDS)
  t_key_values  m_request_body_parameters; // has precedence over m_request_body
  MIME          m_request_mime;            // has precedence over m_request_body and m_request_body_parameters
  //
  // Data retrieved from the request response
  // Received headers and body are stored the WrapperBase
  std::string     m_response_redirect_url;
  std::string     m_response_content_type;
  //
  // Extra curl handles used when sending the request
  curl_slist *    m_curl_headers = nullptr;  // must be persistent (CURLOPT_HTTPHEADER)
  curl_mime *     m_curl_mime    = nullptr;  // must be persistent (CURLOPT_MIMEPOST)
  //
  // Release extra curl handles that were used during the operation
  void release_curl_extras();
  //
  // Set the request method
  bool fill_method();
  //
  // Add all headers to the request
  bool fill_headers();
  //
  // Build the body
  bool fill_body();
  //
  // Build the MIME body
  bool fill_body_mime();
  //
  // Encode parameters into a application/x-www-form-urlencoded string
  std::string encode_parameters( const t_key_values & p_parameters );
  //
  // Add parameters as query parameters to the given URL
  std::string url_with_parameters( const std::string & p_url, const t_key_values & p_parameters );
  //
  // When using external JSON parser, once the JSON text is generated, prepare the query
  HTTP & REST( const std::string & p_uri, const std::string & p_verb, std::string && p_body );
};

} // namespace curlev
