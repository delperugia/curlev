/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <curl/curl.h>
#include <string>
#include <variant>
#include <vector>

namespace curlev
{
  namespace mime
  {
    // The 3 types of data that can be added in the MIME document
    // - content_type is the optional part's content type
    // - filedata is a file read to retrieve the data to sent, and is used as the default filename
    // - filename is the optional remote filename
    struct parameter { std::string name; std::string value; };
    struct data      { std::string name; std::string data;     std::string content_type; std::string filename; };
    struct file      { std::string name; std::string filedata; std::string content_type; std::string filename; };
    //
    // The structuring of the MIME document:
    // parts:        the MIME document, it is a vector of parts (part)
    // part:         either a data (parameter, data, file) or an alternative of parts (alternatives)
    // alternatives: a vector of alternatives (alternative)
    // alternative:  a data (parameter, data, file)
    using  alternative  = std::variant< parameter, data, file >;
    using  alternatives = std::vector < alternative >;
    using  part         = std::variant< parameter, data, file, alternatives >;
    using  parts        = std::vector < part >;
  } // namespace mime

//--------------------------------------------------------------------
// This class is used to create a MIME document
class MIME
{
public:
  // Add one or several parts to the MIME document
  void add_parts( const mime::parts & p_parts );
  //
  // Apply parts to curl easy handle
  bool apply( CURL * p_curl, curl_mime * p_curl_mime ) const;
  //
  // Resets all parts added.
  void clear() { m_parts.clear(); };
  //
  // Returns true if the MIME document is empty
  [[nodiscard]] bool empty() const { return m_parts.empty(); }
  //
private:
  mime::parts m_parts;
};

} // namespace curlev
