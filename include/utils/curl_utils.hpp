/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <curl/curl.h>
#include <string>

namespace curlev
{

// Wrappers around the libcurl setop functions, returning true upon success

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

// Add std::string to a curl_slist. The first time p_list must be set to nullptr.
// p_list is updated at each call, and must be freed using curl_slist_free_all.
// p_string must not be empty.
bool curl_slist_checked_append( curl_slist *& p_list, const std::string & p_string );

} // namespace curlev
