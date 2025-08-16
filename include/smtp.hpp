/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <cstddef>
#include <curl/curl.h>
#include <future>
#include <string>

#include "mime.hpp"
#include "wrapper.hpp"

namespace curlev
{

  namespace smtp
  {
    // An email, possibly with a display name
    struct address
    {
      std::string address_spec; // john.smith@example.org
      std::string display_name; // John Smith
      //
      address() = default;
      //
      // Accepts address with or without display name:
      // - Mary Smith <mary@x.test>
      // - "Mary Smith" <mary@x.test>"
      // - <mary@x.test>
      // - mary@x.test
      explicit address( std::string_view p_text );
      //
      void clear();
      //
      // Returns the address enclosed in angle brackets, possibly with a quoted display name
      [[nodiscard]] std::string get_name_addr() const; // "Mary Smith" <mary@x.test> or <jdoe@example.org>
      //
      // Returns the address enclosed in angle brackets
      [[nodiscard]] std::string get_addr_spec() const; // <mary@x.test>
    };

    using recipients = std::vector< address >;
  } // namespace smtp

//--------------------------------------------------------------------
// A specialization of the Wrapper for the SMTP protocol.
// It supports sending emails with optional attachments and headers.
// The body can be specified as raw content or as a MIME document.
class SMTP : public Wrapper< SMTP >
{
public:
  ~SMTP() override;
  //
  // The first step is to call one of these methods:
  //
  // [1] Send an email with MIME parts (attachments, HTML, etc.)
  SMTP & SEND(
      const std::string &      p_url,
      const smtp::address &    p_from,
      const smtp::recipients & p_to,
      const std::string &      p_subject,
      const mime::parts &      p_parts );
  //
  // [2] Send a simple, raw, email
  // p_body should be a rfc5322 message
  SMTP & SEND(
      const std::string &      p_url,
      const smtp::address &    p_from,
      const smtp::recipients & p_to,
      const std::string &      p_body );
  //
  // Add headers to the email. Only for requests [1]
  SMTP & add_headers( const key_values & p_headers );
  //
  struct response
  {
    long code = 0;
  };
  //
  std::future< response > launch();
  //
protected:
  // Prevent creating directly an instance of the class, the Wrapper::create() method must be used
  explicit SMTP( ASync & p_async ) : Wrapper< SMTP >( p_async ) {};
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
  std::string      m_request_url;
  smtp::address    m_request_from;
  smtp::recipients m_request_to;
  std::string      m_request_subject;
  key_values       m_request_headers;
  MIME             m_request_mime;
  //
  // Extra curl handles used when sending the request
  curl_slist *     m_curl_recipients = nullptr; // must be persistent (CURLOPT_MAIL_RCPT)
  curl_slist *     m_curl_headers    = nullptr; // must be persistent (CURLOPT_HTTPHEADER)
  curl_mime *      m_curl_mime       = nullptr; // must be persistent (CURLOPT_MIMEPOST)
  //
  // Release extra curl handles that were used during the operation
  void release_curl_extras();
  //
  // Add all recipients to the request
  bool fill_recipients();
  //
  // Add all headers to the request
  bool fill_headers();
  //
  // Build the MIME body
  bool fill_body_mime();
  //
  // Build the body
  bool fill_body();
};

} // namespace curlev
