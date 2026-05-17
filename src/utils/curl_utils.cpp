/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "utils/curl_utils.hpp"
#include "utils/string_utils.hpp"

namespace curlev
{

//--------------------------------------------------------------------
// Add std::string to a curl_slist. The first time p_list must be set to nullptr.
// p_list is updated at each call, and must be freed using curl_slist_free_all.
// p_string must not be empty.
// Because curl_slist_append returns nullptr on error, the previous value
// of p_list must be preserved to be later freed.
bool curl_slist_checked_append( curl_slist *& p_list, const std::string & p_string )
{
  if ( p_string.empty() ) // nothing to do
    return false;
  //
  curl_slist * temp = curl_slist_append( p_list, p_string.c_str() ); // doesn't have to be persistent
  if ( temp == nullptr ) // allocation failed
    return false;
  //
  p_list = temp;
  return true;
}

//--------------------------------------------------------------------
// Add a header to the list. The first time p_list must be set to nullptr.
// p_list is updated at each call, and must be freed using curl_slist_free_all.
// Ensure p_key and p_value can't cause header injection.
bool curl_header_checked_append(
    curl_slist *&    p_headers,
    std::string_view p_key,
    std::string_view p_value )
{
  if ( ! is_safe_header_component( p_key ) || ! is_safe_header_component( p_value ) )
    return false;
  //
  std::string result;
  result.reserve( p_key.size() + 2 + p_value.size() );
  result += p_key;
  result += ": ";
  result += p_value;
  //
  return curl_slist_checked_append( p_headers, result );
}

} // namespace curlev
