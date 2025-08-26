/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <cstddef>
#include <curl/curl.h>
#include <future>
#include <string>

#include "utils/map_utils.hpp"
#include "async.hpp"
#include "mime.hpp"
#include "wrapper.hpp"

namespace curlev
{

  namespace smtp
  {
    // An email, possibly with a display name
    struct address
    {
      enum class Mode { to, cc, bcc };
      //
      std::string address_spec;    // john.smith@example.org
      std::string display_name;    // John Smith
      Mode        mode = Mode::to; // type of recipient
      //
      address() = default;
      //
      // Accepts address with or without display name:
      // - Mary Smith <mary@x.test>
      // - "Mary Smith" <mary@x.test>"
      // - <mary@x.test>
      // - mary@x.test
      // p_mode is only used when using the [1] form of SEND
      explicit address( std::string_view p_text, Mode p_mode = Mode::to );
      //
      // Same syntax as constructor
      address & operator=( std::string_view p_text );
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
  // The first step is to call the SEND method:
  SMTP & SEND(
      const std::string &   p_url,
      const smtp::address & p_from );
  //
  // Add headers to the email. Only for MIME emails
  SMTP & add_headers( const key_values & p_headers );
  //
  // Adding body to a request. Only one of them must be used.
  //
  SMTP & set_mime(
      const smtp::recipients & p_to,
      const std::string &      p_subject,
      const mime::parts &      p_parts );
  //
  SMTP & set_body(
      const smtp::recipients & p_to,
            std::string &&     p_body );
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
  // Extra curl handles used when sending the request
  curl_slist * m_curl_recipients = nullptr; // must be persistent (CURLOPT_MAIL_RCPT)
  curl_slist * m_curl_headers    = nullptr; // must be persistent (CURLOPT_HTTPHEADER)
  curl_mime *  m_curl_mime       = nullptr; // must be persistent (CURLOPT_MIMEPOST)
  //
  smtp::address m_request_sender;  // the From field
  //
  // Release extra curl handles that were used during the operation
  void release_curl_extras();
  //
  // Add all recipients to the request
  bool fill_recipients( const smtp::recipients & p_to );
  //
  // Add all headers to the request
  bool fill_headers(
      const std::string &      p_subject,
      const smtp::address &    p_from,
      const smtp::recipients & p_recipients );
  //
  // Build the MIME body
  bool fill_body_mime( const mime::parts & p_parts );
  //
  // Set the received raw body
  bool fill_body( std::string && p_body );
};

} // namespace curlev
