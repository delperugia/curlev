/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <algorithm>
#include <charconv>
#include <cstring>
#include <functional>
#include <sstream>

#include "common.hpp"

namespace curlev
{

//--------------------------------------------------------------------
// Calculate a case insensitive hash optimized for HTTP header keys
// (2 to 30 characters, a-z and - characters, limited number of headers).
// For the 50 most common headers there is no collision
std::size_t t_ci::operator()( const std::string & p_key ) const
{
  std::size_t hash = 0;
  //
  for ( char c : p_key )
    hash = 31 * hash + tolower( c ); // cppcheck-suppress useStlAlgorithm
  //
  return hash;
}

//--------------------------------------------------------------------
// std::lexicographical_compare is usually used but here
// when only deal with US-ASCII (HTTP header keys) and
// strcasecmp is ~5 times faster
bool t_ci::operator()( const std::string & p_a, const std::string & p_b ) const
{
  return strcasecmp( p_a.c_str(), p_b.c_str() ) == 0;
}

//--------------------------------------------------------------------
// Start with p_list set to nullptr, then add string, p_list is updated
// and must be freed using curl_slist_free_all.
// p_string must not be empty.
// Because curl_slist_append returns nullptr on error, the previous value
// of p_list must be kept to be preserved to be freed.
bool curl_slist_checked_append( curl_slist *& p_list, const std::string & p_string )
{
  if ( p_string.empty() )
    return false;
  //
  curl_slist * temp = curl_slist_append( p_list, p_string.c_str() );
  //
  if ( temp == nullptr )	
  {
    return false;
  }
  else
  {
    p_list = temp;
    return true;
  }
}

//--------------------------------------------------------------------
// Converts a std::string_view to an long. Returns 0 on error.
long svtol( std::string_view p_string )
{
  long value;
  auto last   = p_string.data() + p_string.size();
  auto result = std::from_chars( p_string.data(), last, value );
  //
  if ( result.ec == std::errc() && result.ptr == last ) // ok and no remaining character
    return value;
  else
    return 0;
}

//--------------------------------------------------------------------
// Remove leading and trailing white spaces (space, tabulations...) from string
std::string_view trim( std::string_view p_string )
{
  auto begin = std::find_if_not( p_string.begin() , p_string.end() , ::isspace );
  auto end   = std::find_if_not( p_string.rbegin(), p_string.rend(), ::isspace ).base();
  //
  return ( begin < end ) ? std::string_view( begin, end - begin ) : std::string_view( "" );
}

//--------------------------------------------------------------------
// Parse a key-value comma-separated string (CSKV) and call the handler for each pair.
// The handler must return false if the key-value pair is invalid.
bool parse_cskv( const std::string &                                                               p_options,
                 const std::function< bool( std::string_view p_key, std::string_view p_value ) > & p_handler )
{
  std::istringstream iss( p_options );
  std::string        token;
  //
  while ( std::getline( iss, token, ',' ) )
  {
    std::string_view key_value = token;
    //
    auto delimiter_pos = key_value.find( '=' );
    if ( delimiter_pos == std::string::npos )	
      return false; // invalid format: no = sign
    //
    auto key   = trim( key_value.substr( 0, delimiter_pos ) );
    auto value = trim( key_value.substr( delimiter_pos + 1 ) );
    //
    if ( ! p_handler( key, value ) )
      return false; // invalid option reported
  }
  //
  return true;
}

} // namespace curlev
