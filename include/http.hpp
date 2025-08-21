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
  // The first step is to call the REQUEST method or one of the convenient wrappers.
  // Query parameters can be added to the URL.
  //
  HTTP & REQUEST( const std::string & p_method, std::string p_url, const key_values &  p_query_parameters = {} );
  //
  HTTP & GET   ( const std::string & p_url, const key_values & p_qp = {} ) { return REQUEST( "GET"   , p_url, p_qp ); }
  HTTP & DELETE( const std::string & p_url, const key_values & p_qp = {} ) { return REQUEST( "DELETE", p_url, p_qp ); }
  HTTP & PATCH ( const std::string & p_url, const key_values & p_qp = {} ) { return REQUEST( "PATCH" , p_url, p_qp ); }
  HTTP & POST  ( const std::string & p_url, const key_values & p_qp = {} ) { return REQUEST( "POST"  , p_url, p_qp ); }
  HTTP & PUT   ( const std::string & p_url, const key_values & p_qp = {} ) { return REQUEST( "PUT"   , p_url, p_qp ); }
  //
  // Working with REST (JSON PATCH, POST and PUT requests).
  // Query parameters can be added to the URL.
  #if __has_include( <nlohmann/json.hpp> )
    HTTP & REST( const std::string & p_url, const std::string & p_verb, const nlohmann::json & p_json, const key_values & p_query_parameters = {} );
  #endif
  #if __has_include( <rapidjson/document.h> )
    HTTP & REST( const std::string & p_url, const std::string & p_verb, const rapidjson::Document & p_json, const key_values & p_query_parameters = {} );
  #endif
  //
  // Add headers to the request
  HTTP & add_headers( const key_values & p_headers );
  //
  // Adding body to a request. Only one of them must be used.
  //
  HTTP & set_body      ( const std::string & p_content_type, std::string && p_body );
  HTTP & set_parameters( const key_values &  p_body_parameters );
  HTTP & set_mime      ( const mime::parts & p_parts );
  //
  // These accessors return references for efficiency, directly exposing the object's
  // internal. They must only be used after the request has fully completed to ensure data
  // consistency and avoid unstable values during ongoing operations.
  const key_values_ci & get_headers     () const noexcept;
  const std::string   & get_content_type() const noexcept;
  const std::string   & get_redirect_url() const noexcept;
  const std::string   & get_body        () const noexcept;
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
  struct response
  {
    long          code = 0;
    key_values_ci headers;
    std::string   redirect_url;
    std::string   content_type;
    std::string   body;
    //
    #if __has_include( <nlohmann/json.hpp> )
      bool get_json( nlohmann::json & p_json ) const noexcept;
    #endif
    #if __has_include( <rapidjson/document.h> )
      bool get_json( rapidjson::Document & p_json ) const noexcept;
    #endif
  };
  //
  std::future< response > launch();
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
  // Data retrieved from the request response
  // Received headers and body are stored the WrapperBase
  std::string m_response_content_type;
  std::string m_response_redirect_url;
  //
  // Extra curl handles used when sending the request
  curl_slist * m_curl_headers = nullptr;  // must be persistent (CURLOPT_HTTPHEADER)
  curl_mime *  m_curl_mime    = nullptr;  // must be persistent (CURLOPT_MIMEPOST)
  //
  // Release extra curl handles that were used during the operation
  void release_curl_extras();
};

} // namespace curlev
