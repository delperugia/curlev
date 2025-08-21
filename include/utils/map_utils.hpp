/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <string>
#include <unordered_map>

namespace curlev
{

// Case insensitive operations for the std::unordered_map of key_values_ci
struct hash_ci
{
  std::size_t operator()( const std::string & p_key ) const;
  bool        operator()( const std::string & p_a, const std::string & p_b ) const;
};

// Used to convey parameters or headers (unordered_map are almost twice as fast as map)
using key_values    = std::unordered_map< std::string, std::string >;
using key_values_ci = std::unordered_map< std::string, std::string, hash_ci, hash_ci >;

// Converts keys and values in parameters and add them to the given p_text.
// Each key/value is prepended by a separator:
//  - p_first_separator between p_text and the firs key/value
//  - p_subsequent_separator between subsequent keys/values
// p_first_separator and p_subsequent_separator can be set to 0 to add nothing.
void append_url_encoded(
    std::string &      p_text,
    const key_values & p_parameters,
    char               p_first_separator      = 0,
    char               p_subsequent_separator = '&');

} // namespace curlev
