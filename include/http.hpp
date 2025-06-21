/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <cstddef>
#include <cstring>
#include <curl/curl.h>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "wrapper.hpp"

namespace curlev
{

// A specialization of the Wrapper for the HTTP protocol.
// It supports POST, PUT, PATCH, GET and DELETE methods, with parameters in query.
// For POST, PUT, PATCH a body can be specified using one of:
//  - a raw content type and body
//  - encoded parameters (content type application/x-www-form-urlencoded)
//  - a MIME document of parameters and documents
// Headers can be added if needed.
// Class must be cleared between requests.

class HTTP : public Wrapper< HTTP >
{
public:
  // Case insensitive comparison for the receive headers
  // std::lexicographical_compare is usually used but here
  // when only deal with US-ASCII (HTTP header keys) and
  // strcasecmp is ~5 times faster
  struct iCompare
  {
    bool operator()( const std::string & a, const std::string & b ) const
    {
      return strcasecmp( a.c_str(), b.c_str() ) < 0;
    }
  };
  //
  // Used to convey parameters or headers
  using t_key_values  = std::map< std::string, std::string >;             // used for sent parameters and headers
  using t_ikey_values = std::map< std::string, std::string, iCompare >;   // used for received headers, case insensitive keys
  //
  // When sending a MIME document. MIME body can contain a list of regular parameters
  // (key / value) or files. t_mime_parts is a vector of variants. Each variant
  // represents either a parameter or a file. If filename of t_mime_file is not set,
  // it behaves like a t_mime_parameter
  using t_mime_parameter = std::tuple< std::string, std::string >; // name, value
  using t_mime_file      = std::tuple< std::string, std::string, std::string, std::string >; // name, type, data, filename
  using t_mime_part      = std::variant< t_mime_parameter, t_mime_file >;
  using t_mime_parts     = std::vector< t_mime_part >;
  //
  virtual ~HTTP();
  //
  // The first step is to call one of theses method:
  //
  // Simple GET and DELETE, with optional query parameters
  HTTP & GET   ( const std::string & p_url, const t_key_values & p_query_parameters = {} );
  HTTP & DELETE( const std::string & p_url, const t_key_values & p_query_parameters = {} );
  //
  // [1] Requests with a body possibly added later
  HTTP & POST ( const std::string & p_url ) { return POST ( p_url, "", "" ); }
  HTTP & PUT  ( const std::string & p_url ) { return PUT  ( p_url, "", "" ); }
  HTTP & PATCH( const std::string & p_url ) { return PATCH( p_url, "", "" ); }
  //
  // [2] Requests with a raw body
  HTTP & POST ( const std::string & p_url, const std::string & p_content_type, const std::string & p_body );
  HTTP & PUT  ( const std::string & p_url, const std::string & p_content_type, const std::string & p_body );
  HTTP & PATCH( const std::string & p_url, const std::string & p_content_type, const std::string & p_body );
  //
  // [3] Requests with a body made of url encoded parameters
  HTTP & POST ( const std::string & p_url, const t_key_values & p_body_parameter );
  HTTP & PUT  ( const std::string & p_url, const t_key_values & p_body_parameter );
  HTTP & PATCH( const std::string & p_url, const t_key_values & p_body_parameter );
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
  // Add MIME parameters. Only for requests [1]
  HTTP & add_mime_parameters( const t_mime_parts & p_mime_parts );
  //
  // Accessors to be used after a request
  t_ikey_values get_headers     ( void ) const;
  std::string   get_content_type( void ) const;
  std::string   get_redirect_url( void ) const;
  std::string   get_body        ( void ) const;
  //
protected:
  // Prevent creating directly an instance of the class, the Wrapper::create() method must be used
  explicit HTTP( ASync & p_async ) : Wrapper< HTTP >( p_async ) {};
  //
  // Called by Wrapper before starting a request
  bool prepare_protocol( void ) override;
  //
  // Called by Wrapper after a request completes to retrieve protocol specific details
  void finalize_protocol( void ) override;
  //
  // Called by Wrapper when the user wants to reset the Protocol
  void clear_protocol( void ) override;
  //
private:
  // Data used when sending the request
  enum class Method { none, eGET, eDELETE, ePOST, ePUT, ePATCH }
                m_request_method = Method::none;
  std::string   m_request_url;
  t_key_values  m_request_query_parameters;
  t_key_values  m_request_headers;
  std::string   m_request_content_type;
  std::string   m_request_body;            // must be persistent (CURLOPT_POSTFIELDS)
  t_key_values  m_request_body_parameters; // has precedence over m_request_body
  t_mime_parts  m_request_mime;            // has precedence over m_request_body and m_request_body_parameters
  //
  // Data retrieved from the request response
  t_ikey_values m_response_headers;        // must be persistent (CURLOPT_HEADERDATA)
  std::string   m_response_redirect_url;
  std::string   m_response_content_type;
  std::string   m_response_body;           // must be persistent (CURLOPT_WRITEDATA)
  //
  // Extra curl handles used when sending the request
  curl_slist *  m_curl_headers = nullptr;  // must be persistent (CURLOPT_HTTPHEADER)
  curl_mime *   m_curl_mime    = nullptr;  // must be persistent (CURLOPT_MIMEPOST)
  //
  // Release extra curl handles that were used during the operation
  void release_curl_extras( void );
  //
  // Set the request method
  bool fill_method( void );
  //
  // Add all headers to the request
  bool fill_headers( void );
  //
  // Build the body
  bool fill_body( void );
  //
  // Build the MIME body
  bool fill_body_mime( void );
  //
  // Encode parameters into a application/x-www-form-urlencoded string
  std::string encode_parameters( const t_key_values & p_parameters );
  //
  // Add parameters as query parameters to the given URL
  std::string url_with_parameters( const std::string & p_url, const t_key_values & p_parameters );
};

} // namespace curlev
