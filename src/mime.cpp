/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "mime.hpp"

namespace curlev
{

namespace
{
  // Convenient functions that only set the option if the parameter is not empty

  bool mime_name( curl_mimepart * p_mime_part, const std::string & p_name )
  {
    return p_name.empty() || CURLE_OK == curl_mime_name( p_mime_part, p_name.c_str() );
  }

  bool mime_type( curl_mimepart * p_mime_part, const std::string & p_type )
  {
    return p_type.empty() || CURLE_OK == curl_mime_type( p_mime_part, p_type.c_str() );
  }

  bool mime_filename( curl_mimepart * p_mime_part, const std::string & p_filename )
  {
    return p_filename.empty() || CURLE_OK == curl_mime_filename( p_mime_part, p_filename.c_str() );
  }

  bool mime_filedata( curl_mimepart * p_mime_part, const std::string & p_filedata )
  {
    return p_filedata.empty() || CURLE_OK == curl_mime_filedata( p_mime_part, p_filedata.c_str() );
  }

  bool mime_data( curl_mimepart * p_mime_part, const std::string & p_data )
  {
    return p_data.empty() || CURLE_OK == curl_mime_data( p_mime_part, p_data.c_str(), p_data.length() );
  }
} // namespace

namespace
{
  // The 4 functions to visit the variant mime::part and mime::alternative

  bool read_part( CURL * /* easy */, curl_mimepart * p_mime_part, const mime::parameter & p_parameter )
  {
    return mime_name( p_mime_part, p_parameter.name  ) &&
           mime_data( p_mime_part, p_parameter.value );
  }

  bool read_part( CURL * /* easy */, curl_mimepart * p_mime_part, const mime::data & p_data )
  {
    return mime_name    ( p_mime_part, p_data.name         ) &&
           mime_data    ( p_mime_part, p_data.data         ) &&
           mime_type    ( p_mime_part, p_data.content_type ) &&
           mime_filename( p_mime_part, p_data.filename     );
  }

  bool read_part( CURL * /* easy */, curl_mimepart * p_mime_part, const mime::file & p_file )
  {
    return mime_name    ( p_mime_part, p_file.name         ) &&
           mime_filedata( p_mime_part, p_file.filedata     ) &&
           mime_type    ( p_mime_part, p_file.content_type ) &&
           mime_filename( p_mime_part, p_file.filename     );
  }

  bool read_part( CURL * p_curl, curl_mimepart * p_mime_part, const mime::alternatives & parts )
  {
    curl_mime * alternative = curl_mime_init( p_curl );                 // create a new alternative
    //
    bool ok = true;
    //
    ok = ok && alternative != nullptr;
    //
    for ( const auto & part : parts )
    {
      curl_mimepart * sub_part = curl_mime_addpart( alternative );      // add a part to the alternative
      //
      ok = ok && sub_part != nullptr;
      //
      std::visit( [ &ok, p_curl, sub_part ]( const auto & visited ) {
                    ok = ok && read_part( p_curl, sub_part, visited );  // and read the part
                  },
                  part );
    }
    //
    ok = ok && CURLE_OK == curl_mime_type    ( p_mime_part, "multipart/alternative" );
    ok = ok && CURLE_OK == curl_mime_subparts( p_mime_part, alternative );
    //
    if ( ! ok )
      curl_mime_free( alternative ); // ok on nullptr
    //
    return ok;
  }
} // namespace

//--------------------------------------------------------------------
// Apply the configured parts to the given CURL easy handle.
// It returns false if any option fails to set.
bool MIME::apply( CURL * p_curl, curl_mime * p_curl_mime ) const
{
  bool ok = true;
  //
  for ( const auto & part : m_parts )
  {
    curl_mimepart * mime_part = curl_mime_addpart( p_curl_mime );     // create a part
    //
    ok = ok && mime_part != nullptr;
    //
    std::visit( [ &ok, p_curl, mime_part ]( const auto & visited ) {
                  ok = ok && read_part( p_curl, mime_part, visited ); // and read it
                },
                part );
  }
  //
  return ok;
}

//--------------------------------------------------------------------
// Add several parts to the MIME document
void MIME::add_parts( const mime::parts & p_parts )
{
  m_parts.insert( m_parts.end(), p_parts.begin(), p_parts.end() );
}

} // namespace curlev
