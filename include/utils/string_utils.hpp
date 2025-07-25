/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <string>
#include <string_view>

namespace curlev
{

// Remove leading and trailing white spaces (space, tabulations...) from a string
std::string_view trim( std::string_view p_string );

// Converts a std::string_view to a long. Returns false on error.
bool svtol( std::string_view p_string, long & p_value );

// Converts a std::string_view to an unsigned long. Returns false on error.
bool svtoul( std::string_view p_string, unsigned long & p_value );

// Checks is 2 ASCII strings are equal, ignoring case differences
bool equal_ascii_ci( const std::string & p_a, const std::string & p_b );

//--------------------------------------------------------------------
// Parse a key-value comma-separated string (CSKV) and call the handler for each pair.
// The handler receives two string_view and must return false if the key-value pair is invalid.
template < typename Callable >
bool parse_cskv( std::string_view p_cskv, Callable && p_handler )
{
  while ( ! p_cskv.empty() )
  {
    auto comma_pos = p_cskv.find( ',' );
    auto key_value = p_cskv.substr( 0, comma_pos );
    //
    p_cskv.remove_prefix( comma_pos == std::string_view::npos ? p_cskv.size() : comma_pos + 1 );
    //
    auto delimiter_pos = key_value.find( '=' );
    if ( delimiter_pos == std::string_view::npos )
      return false; // invalid format: no = sign
    //
    auto key   = trim( key_value.substr( 0, delimiter_pos  ) );
    auto value = trim( key_value.substr( delimiter_pos + 1 ) );
    //
    if ( ! std::forward< Callable >( p_handler )( key, value ) )
      return false; // invalid option reported
  }
  //
  return true;
}

} // namespace curlev
