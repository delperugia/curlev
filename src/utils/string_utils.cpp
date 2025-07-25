/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <algorithm>
#include <cstring>
#include <limits>

#include "utils/string_utils.hpp"

namespace curlev
{

namespace
{
  bool is_digit( char p_character )
  {
    return std::isdigit( static_cast< unsigned char >( p_character ) ) != 0;
  }

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

//--------------------------------------------------------------------
// Remove leading and trailing white spaces (space, tabulations...) from a string
std::string_view trim( std::string_view p_string )
{
  const auto * begin = std::find_if_not( p_string.begin() , p_string.end() , ::isspace );
  const auto * end   = std::find_if_not( p_string.rbegin(), p_string.rend(), ::isspace ).base();
  //
  return ( begin < end ) ? std::string_view( begin, end - begin ) : std::string_view();
}

//--------------------------------------------------------------------
// Converts a std::string_view to a long. Returns false on error.
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

//--------------------------------------------------------------------
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
bool equal_ascii_ci( const std::string & p_a, const std::string & p_b )
{
  return p_a.length() == p_b.length() &&
         strcasecmp( p_a.c_str(), p_b.c_str() ) == 0;
}

} // namespace curlev
