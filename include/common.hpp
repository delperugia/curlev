/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <curl/curl.h>
#include <functional>
#include <unordered_map>
#include <string>

namespace curlev
{

//--------------------------------------------------------------------
// Remove leading and trailing white spaces (space, tabulations...) from a string
inline
std::string_view trim( std::string_view p_string )
{
  const auto * begin = std::find_if_not( p_string.begin() , p_string.end() , ::isspace );
  const auto * end   = std::find_if_not( p_string.rbegin(), p_string.rend(), ::isspace ).base();
  //
  return ( begin < end ) ? std::string_view( begin, end - begin ) : std::string_view();
}

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
    auto key   = trim( key_value.substr( 0, delimiter_pos ) );
    auto value = trim( key_value.substr( delimiter_pos + 1 ) );
    //
    if ( ! std::forward< Callable >( p_handler )( key, value ) )
      return false; // invalid option reported
  }
  //
  return true;
}

// Converts a std::string_view to a long. Returns false on error.
bool svtol( std::string_view p_string, long & p_value );

// Converts a std::string_view to an unsigned long. Returns false on error.
bool svtoul( std::string_view p_string, unsigned long & p_value );

// Checks is 2 ASCII strings are equal, ignoring case differences
bool equal_ascii_ci( const std::string & p_a, const std::string & p_b );

// Case insensitive operations for the std::unordered_map of t_key_values_ci
struct t_ci
{
  std::size_t operator()( const std::string & p_key ) const;
  bool        operator()( const std::string & p_a, const std::string & p_b ) const;
};
//
// Used to convey parameters or headers (unordered_map are almost twice as fast as map)
using t_key_values    = std::unordered_map< std::string, std::string >;             // used for sent parameters and headers
using t_key_values_ci = std::unordered_map< std::string, std::string, t_ci, t_ci >; // used for received headers, case insensitive keys

// Wrapper around the libcurl setops function, returning true upon success
template < typename Type >
bool easy_setopt( CURL * p_curl, CURLoption p_option, Type && p_parameter )
{
  return CURLE_OK == curl_easy_setopt( p_curl, p_option, std::forward< Type >( p_parameter ) );
}

template < typename Type >
bool multi_setopt( CURL * p_curl, CURLMoption p_option, Type && p_parameter )
{
  return CURLM_OK == curl_multi_setopt( p_curl, p_option, std::forward< Type >( p_parameter ) );
}

template < typename Type >
bool share_setopt( CURL * p_curl, CURLSHoption p_option, Type && p_parameter )
{
  return CURLSHE_OK == curl_share_setopt( p_curl, p_option, std::forward< Type >( p_parameter ) );
}

// Start with p_list set to nullptr, then add string.
// p_list is updated and must be freed using curl_slist_free_all.
// p_string must not be empty.
bool curl_slist_checked_append( curl_slist *& p_list, const std::string & p_string );

// To make a class non-copyable and non-movable, inherit from this class
class non_transferable
{
public:
  virtual ~non_transferable()                              = default;
  //
  non_transferable            ( const non_transferable & ) = delete;
  non_transferable & operator=( const non_transferable & ) = delete;
  non_transferable            ( non_transferable && )      = delete;
  non_transferable & operator=( non_transferable && )      = delete;
  //
protected:
  non_transferable()                                       = default;
};

} // namespace curlev
