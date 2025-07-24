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
// Converts a std::string_view to a long. Returns false on error.
namespace
{
  bool is_digit( char p_character )
  {
    return std::isdigit( static_cast< unsigned char >( p_character ) ) != 0;
  }
} // namespace

// ~ twice faster than using std::stol, and works on string_view
bool svtol( std::string_view p_string, long & p_value )
{
  constexpr auto base     = 10U;
  unsigned long  max      = std::numeric_limits< long >::max(); // 2147483647
  //
  unsigned long  value    = 0;
  bool           negative = false;
  const auto *   current  = p_string.data();
  const auto *   last     = p_string.data() + p_string.size();
  //
  if ( current < last && *current == '-' )
  {
    negative = true;
    max++; // now 2147483648
    current++;
  }
  //
  if ( current == last ) // p_string is "-" or ""
    return false;
  //
  while ( current < last && is_digit( *current ) )
  {
    value = value * base + ( *current++ - '0' );
    if ( value > max )
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

// The unsigned version of svtol
bool svtoul( std::string_view p_string, unsigned long & p_value )
{
  constexpr auto base     = 10U;
  constexpr auto max      = std::numeric_limits< unsigned long >::max(); // 4294967295
  //
  unsigned long  value    = 0;
  const auto *   current  = p_string.data();
  const auto *   last     = p_string.data() + p_string.size();
  //
  if ( current == last ) // p_string is ""
    return false;
  //
  while ( current < last && is_digit( *current ) )
  {
    unsigned long digit = *current - '0';
    if ( value > max / base )
      return false;
    value *= base;
    if ( value > max - digit )
      return false;
    value += digit;
    current++;
  }
  //
  if ( current < last ) // non digit found in p_string
    return false;
  //
  p_value = value;
  return true;
}

//--------------------------------------------------------------------
// Checks is two ASCII strings are equal, ignoring case differences,
// strcasecmp is ~5 times faster than std::lexicographical_compare.
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

bool equal_ascii_ci( const std::string & p_a, const std::string & p_b )
{
  return p_a.length() == p_b.length() &&
         strcasecmp( p_a.c_str(), p_b.c_str() ) == 0;
}

//--------------------------------------------------------------------
// Calculate a case insensitive hash optimized for HTTP header keys
// (2 to 30 characters, a-z and - characters, limited number of headers).
// For the 50 most common headers there is no collision
std::size_t t_ci::operator()( const std::string & p_key ) const
{
  constexpr std::size_t multiplier = 31;
  std::size_t           hash       = 0;
  //
  for ( char c : p_key )
    hash = multiplier * hash + tolower( c ); // cppcheck-suppress useStlAlgorithm
  //
  return hash;
}

//--------------------------------------------------------------------
// HTTP header keys use ASCII-US, so there is no need for std::lexicographical_compare
bool t_ci::operator()( const std::string & p_a, const std::string & p_b ) const
{
  return equal_ascii_ci( p_a, p_b );
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

} // namespace curlev
