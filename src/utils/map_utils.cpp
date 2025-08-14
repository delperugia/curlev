/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <cctype>

#include "utils/map_utils.hpp"
#include "utils/string_utils.hpp"

namespace curlev
{

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

} // namespace curlev
