#pragma once

#include <cstddef>
#include <curl/curl.h>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "wrapper.hpp"

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
        // Used to convey parameters or headers
        using t_key_values     = std::map< std::string, std::string >;
        //
        // When sending a MIME document
        using t_mime_parameter = std::tuple< std::string, std::string >;  // name, value
        using t_mime_file      = std::tuple< std::string, std::string, std::string, std::string >;  // name, type, data, filename
        using t_mime_part      = std::variant< t_mime_parameter, t_mime_file >;
        using t_mime_parts     = std::vector< t_mime_part >;
        //
        ~HTTP();
        //
        // Add headers to the next request
        HTTP & add_header ( const std::string & p_header, const std::string & p_value );
        HTTP & add_headers( t_key_values p_headers );
        //
        // Sending a raw body
        HTTP & do_post ( const std::string & p_url, const std::string & p_content_type, const std::string & p_body, const t_key_values & p_query_parameters = {} );
        HTTP & do_put  ( const std::string & p_url, const std::string & p_content_type, const std::string & p_body, const t_key_values & p_query_parameters = {} );
        HTTP & do_patch( const std::string & p_url, const std::string & p_content_type, const std::string & p_body, const t_key_values & p_query_parameters = {} );
        //
        // Sending parameters in body
        HTTP & do_post ( const std::string & p_url, const t_key_values & p_body_parameter = {}, const t_key_values & p_query_parameters = {} );
        HTTP & do_put  ( const std::string & p_url, const t_key_values & p_body_parameter = {}, const t_key_values & p_query_parameters = {} );
        HTTP & do_patch( const std::string & p_url, const t_key_values & p_body_parameter = {}, const t_key_values & p_query_parameters = {} );
        //
        // Sending a MIME body
        HTTP & do_post ( const std::string & p_url, const t_mime_parts & p_mime_parameters, const t_key_values & p_query_parameters = {} );
        HTTP & do_put  ( const std::string & p_url, const t_mime_parts & p_mime_parameters, const t_key_values & p_query_parameters = {} );
        HTTP & do_patch( const std::string & p_url, const t_mime_parts & p_mime_parameters, const t_key_values & p_query_parameters = {} );
        //
        // Simple GET and DELETE
        HTTP & do_get   ( const std::string & p_url, const t_key_values & p_query_parameters = {} );
        HTTP & do_delete( const std::string & p_url, const t_key_values & p_query_parameters = {} );
        //
        // Clear all previous configuration before doing a new request
        HTTP & clear( void ) override;
        //
        // Accessors to be used after a request
        t_key_values         get_headers     ( void ) const { return m_response_headers;      };
        const std::string  & get_content_type( void ) const { return m_response_content_type; };
        const std::string  & get_redirect_url( void ) const { return m_response_redirect_url; };
        const std::string    get_body        ( void ) const { return m_response_body;         };
        //
    protected:
        // Prevent creating directly an instance of the class
        explicit HTTP( ASync & p_async ): Wrapper< HTTP >( p_async ) {};
        //
        // Called by Wrapper after a request to retrieve protocol specific details
        void finalize( void ) override;
        //
    private:
        // Data used when sending the request.
        t_key_values m_request_headers;
        std::string  m_request_body;
        //
        // Data retrieved from the request response.
        t_key_values m_response_headers;
        std::string  m_response_content_type;
        std::string  m_response_redirect_url;
        std::string  m_response_body;
        //
        // Extra curl handles used when sending the request.
        struct curl_slist * m_curl_headers = nullptr;
        curl_mime *         m_curl_mime    = nullptr;
        //
        // All requests without body.
        HTTP & request_no_body( const std::string &  p_method,
                                const std::string &  p_url,
                                const t_key_values & p_query_parameters );
        //
        // Requests with raw or parameters bodies.
        HTTP & request_with_body( const std::string &  p_method,
                                  const std::string &  p_url,
                                  const std::string &  p_content_type,
                                  const std::string &  p_body,
                                  const t_key_values & p_query_parameters );
        //
        // Request with MIME body.
        HTTP & request_with_mime_body( const std::string &  p_method,
                                       const std::string &  p_url,
                                       const t_mime_parts & p_mime_parameters,
                                       const t_key_values & p_query_parameters );
        //
        // Preparing curl before the Wrapper::start.
        bool prepare_curl( const std::string &  p_method,
                           const std::string &  p_url,
                           const t_key_values & p_query_parameters );
        //
        // Release extra curl handles that were used during the operation.
        void release_curl_extras( void );
        //
        // Add all headers to the request.
        bool fill_headers( void );
        //
        // Build the MIME body.
        bool fill_mime( const t_mime_parts & p_mime_parameters );
        //
        // Encode parameters into a application/x-www-form-urlencoded string.
        std::string encode_parameters( const t_key_values & p_parameters );
        //
        // Add parameters as query parameters to the given URL.
        std::string url_with_parameters( const std::string & p_url, const t_key_values & p_parameters );
};
