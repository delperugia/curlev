/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <cctype>

#include "utils/map_utils.hpp"
#include "utils/string_utils.hpp"

namespace curlev
{

// Some magic values: the average "parameter=value" length
constexpr auto c_average_parameter_length = 32;

//--------------------------------------------------------------------
// Calculates a case insensitive hash optimized for HTTP header keys
// (2 to 30 characters, a-z and - characters, limited number of headers).
// For the 50 most common headers there is no collision.
std::size_t hash_ci::operator()( const std::string & p_key ) const
{
  constexpr std::size_t multiplier = 31;
  std::size_t           hash       = 0;
  //
  for ( char c : p_key )
    hash = multiplier * hash + std::tolower( c ); // cppcheck-suppress useStlAlgorithm
  //
  return hash;
}

//--------------------------------------------------------------------
// HTTP header keys use ASCII-US, so there is no need for std::lexicographical_compare
bool hash_ci::operator()( const std::string & p_a, const std::string & p_b ) const
{
  return equal_ascii_ci( p_a, p_b );
}


//--------------------------------------------------------------------
// Converts keys and values in parameters and add them to the given p_text.
// Each key/value is prepended by a separator:
//  - p_first_separator between p_text and the firs key/value
//  - p_subsequent_separator between subsequent keys/values
// p_first_separator and p_subsequent_separator can be set to 0 to add nothing.

namespace
{
  // Appends to p_result the URL encoded string p_string
  void append_string_encoded( std::string & p_text, const std::string & p_string )
  {
    for ( char c : p_string )
    {
      if ( std::isalnum( c ) || c == '-' || c == '.' || c == '_' || c == '~' )
      {
        p_text += c;
      }
      else
      {
        const char * encoded[ 256 ] = {
            "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07", "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
            "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17", "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
            "%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27", "%28", "%29", "%2A", "%2B", "%2C", "%2D", "%2E", "%2F",
            "%30", "%31", "%32", "%33", "%34", "%35", "%36", "%37", "%38", "%39", "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
            "%40", "%41", "%42", "%43", "%44", "%45", "%46", "%47", "%48", "%49", "%4A", "%4B", "%4C", "%4D", "%4E", "%4F",
            "%50", "%51", "%52", "%53", "%54", "%55", "%56", "%57", "%58", "%59", "%5A", "%5B", "%5C", "%5D", "%5E", "%5F",
            "%60", "%61", "%62", "%63", "%64", "%65", "%66", "%67", "%68", "%69", "%6A", "%6B", "%6C", "%6D", "%6E", "%6F",
            "%70", "%71", "%72", "%73", "%74", "%75", "%76", "%77", "%78", "%79", "%7A", "%7B", "%7C", "%7D", "%7E", "%7F",
            "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87", "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
            "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97", "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
            "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7", "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
            "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7", "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
            "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7", "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
            "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7", "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
            "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7", "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
            "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7", "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
        };
        //
        p_text += encoded[ static_cast< unsigned char >( c ) ];
      }
    }
  }
}

void append_url_encoded(
    std::string &      p_text,
    const key_values & p_parameters,
    char               p_first_separator,
    char               p_subsequent_separator )
{
  if ( p_parameters.empty() )
    return;
  //
  p_text.reserve( p_text.length() + p_parameters.size() * c_average_parameter_length );
  //
  char separator = p_first_separator;
  //
  for ( const auto & [ key, value ] : p_parameters )
  {
    if ( separator )
      p_text += separator;
    separator = p_subsequent_separator;
    //
    append_string_encoded( p_text, key );
    p_text += "=";
    append_string_encoded( p_text, value );
  }
}

} // namespace curlev
