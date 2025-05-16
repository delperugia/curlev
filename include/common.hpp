#pragma once

#include <functional>
#include <string>

// Start with p_list set to nullptr, then add string, p_list is updated
// and must be freed using curl_slist_free_all.
bool curl_slist_checked_append( struct curl_slist *& p_list, const std::string & p_string );

// Remove leading and trailing white spaces from string.
std::string trim( const std::string & p_string );

// Parse a key-value comma-separated string (KVCS) and call the handler for each pair.
// The handler must return false if the key-value pair is invalid.
bool parse_cskv( const std::string &                                                       p_options,
                 const std::function< bool( const std::string &, const std::string & ) > & p_handler );
