/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#pragma once

#include <curl/curl.h>
#include <functional>
#include <string>

namespace curlev
{

// Wrapper around the libcurl setops function, returning true upon success
#define easy_setopt(  handle, opt, param ) ( CURLE_OK   == curl_easy_setopt ( handle, opt, param ) )
#define share_setopt( handle, opt, param ) ( CURLSHE_OK == curl_share_setopt( handle, opt, param ) )
#define multi_setopt( handle, opt, param ) ( CURLM_OK   == curl_multi_setopt( handle, opt, param ) )

// Start with p_list set to nullptr, then add string, p_list is updated
// and must be freed using curl_slist_free_all.
// p_string must not be empty.
bool curl_slist_checked_append( curl_slist *& p_list, const std::string & p_string );

// Converts a std::string_view to an long. Returns 0 on error.
long svtol( std::string_view p_string );

// Remove leading and trailing white spaces (space, tabulations...) from string
std::string_view trim( std::string_view p_string );

// Parse a key-value comma-separated string (CSKV) and call the handler for each pair.
// The handler must return false if the key-value pair is invalid.
bool parse_cskv(
    const std::string &                                                               p_options,
    const std::function< bool( std::string_view p_key, std::string_view p_value ) > & p_handler );

} // namespace curlev
