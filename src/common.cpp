#include <algorithm>
#include <curl/curl.h>

#include "common.hpp"

// Start with p_list set to nullptr, then add string, p_list is updated
// and must be freed using curl_slist_free_all.
// Because curl_slist_append returns nullptr on error, the previous value
// of p_list must be kept to be preserved to be freed.
bool curl_slist_checked_append( struct curl_slist *& p_list, const std::string & p_string )
{
    if ( p_string.empty() )
        return false;
    //
    struct curl_slist * temp = curl_slist_append( p_list, p_string.c_str() );
    //
    if ( temp == nullptr )
    {
        return false;
    }
    else
    {
        p_list = temp;
        return true;
    }
}

// Remove leading and trailing white spaces from string.s
std::string trim( const std::string & p_string )
{
    auto begin = std::find_if_not( p_string.begin(), p_string.end(), ::isspace );
    auto end   = std::find_if_not( p_string.rbegin(), p_string.rend(), ::isspace ).base();
    //
    return ( begin < end ) ? std::string( begin, end ) : "";
}
