/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <limits>
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
  constexpr size_t multiplier = 31;
  std::size_t      hash       = 0;
  //
  for ( char c : p_key )
    hash = multiplier * hash + tolower( c ); // cppcheck-suppress useStlAlgorithm
  //
  return hash;
}

//--------------------------------------------------------------------
// std::lexicographical_compare is usually used but here
// when only dealing with US-ASCII (HTTP header keys)
// strcasecmp is ~5 times faster
namespace
{
#ifdef NEED_PORTABLE_STRCASECMP
  // Not a real replacement of strcasecmp: we just need the equality
  int strcasecmp( const char * a, const char * b )
  {
    while ( *a && *b )
      if ( tolower( *a++ ) != tolower( *b++ ) )
        return -1;
    //
    return *a == *b ? 0 : -1;
  }
#endif
} // namespace

bool t_ci::operator()( const std::string & p_a, const std::string & p_b ) const
{
  return strcasecmp( p_a.c_str(), p_b.c_str() ) == 0;
}

//--------------------------------------------------------------------
// Start with p_list set to nullptr, then add string.
// p_list is updated and must be freed using curl_slist_free_all.
// p_string must not be empty.
// Because curl_slist_append returns nullptr on error, the previous value
// of p_list must be kept to be preserved to be freed.
bool curl_slist_checked_append( curl_slist *& p_list, const std::string & p_string )
{
  if ( p_string.empty() ) // nothng to do
    return false;
  //
  curl_slist * temp = curl_slist_append( p_list, p_string.c_str() );
  if ( temp == nullptr ) // allocation failed
    return false;
  //
  p_list = temp;
  return true;
}

//--------------------------------------------------------------------
// Converts a std::string_view to a long. Returns false on error.
namespace
{
  bool is_digit( char p_character )
  {
    return std::isdigit( static_cast< unsigned char >( p_character ) ) != 0;
  }
} // namespace

bool svtol( std::string_view p_string, long & p_value )
{
  constexpr unsigned long base     = 10;
  unsigned long           value    = 0;
  bool                    negative = false;
  const auto *            current  = p_string.data();
  const auto *            last     = p_string.data() + p_string.size();
  //
  if ( current < last && *current == '-' )
  {
    negative = true;
    current++;
  }
  //
  if ( current == last ) // p_string is "-" or ""
    return false;
  //
  while ( current < last && is_digit( *current ) )
  {
    value = value * base + ( *current++ - '0' );
    if ( value > std::numeric_limits< long >::max() )
      return false; // overflow
  }
  //
  if ( current < last ) // non digit found in p_string
    return false;
  //
  p_value = negative ? -static_cast< long >( value ) :
                        static_cast< long >( value );
  return true;
}

//--------------------------------------------------------------------
// Remove leading and trailing white spaces (space, tabulations...) from string
std::string_view trim( std::string_view p_string )
{
  const auto * begin = std::find_if_not( p_string.begin() , p_string.end() , ::isspace );
  const auto * end   = std::find_if_not( p_string.rbegin(), p_string.rend(), ::isspace ).base();
  //
  return ( begin < end ) ? std::string_view( begin, end - begin ) : std::string_view( "" );
}

//--------------------------------------------------------------------
// Parse a key-value comma-separated string (CSKV) and call the handler for each pair.
// The handler must return false if the key-value pair is invalid.
bool parse_cskv(
    const std::string &                                                               p_cskv,
    const std::function< bool( std::string_view p_key, std::string_view p_value ) > & p_handler )
{
  std::istringstream iss( p_cskv );
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
