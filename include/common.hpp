#pragma once

#include <string>

// Start with p_list set to nullptr, then add string, p_list is updated
// and must be freed using curl_slist_free_all.
bool curl_slist_checked_append( struct curl_slist *& p_list, const std::string & p_string );

// Remove leading and trailing white spaces from string.
std::string trim( const std::string & p_string );
