/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <string>
#include <unordered_map>

namespace curlev
{

// Case insensitive operations for the std::unordered_map of t_key_values_ci
struct t_ci
{
  std::size_t operator()( const std::string & p_key ) const;
  bool        operator()( const std::string & p_a, const std::string & p_b ) const;
};

// Used to convey parameters or headers (unordered_map are almost twice as fast as map)
using t_key_values    = std::unordered_map< std::string, std::string >;
using t_key_values_ci = std::unordered_map< std::string, std::string, t_ci, t_ci >;

} // namespace curlev
