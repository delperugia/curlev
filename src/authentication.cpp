#include <sstream>
#include <string>
#include <algorithm>

#include "common.hpp"
#include "authentication.hpp"

// Expect a KVCS list of credential details. Example:
//   mode=basic,user=joe,secret=abc123
// Available keys are:
//   Name       Comment
//   mode       basic, digest or bearer
//   user       for basic and digest only: user login
//   secret     password or token
bool Authentication::set(const std::string& p_options)
{
    try
    {
        std::istringstream iss( p_options );
        std::string        token;
        //
        while ( std::getline( iss, token, ',' ) )
        {
            token = trim( token );
            //
            auto delimiter_pos = token.find( '=' );
            if ( delimiter_pos == std::string::npos )
                return false;  // invalid format
            //
            std::string key   = trim( token.substr( 0, delimiter_pos  ) );
            std::string value = trim( token.substr( delimiter_pos + 1 ) );
            //
            if ( key == "mode" )
            {
                     if ( value == "basic"  ) m_mode = basic;
                else if ( value == "digest" ) m_mode = digest;
                else if ( value == "bearer" ) m_mode = bearer;
                else
                    return false;  // unknown mode
            }
            else if ( key == "user"   ) m_user = value;
            else if ( key == "secret" ) m_secret = value;
            //
            // ignore unknown keys for forward compatibility
        }
    }
    catch ( const std::exception & )
    {
        return false;
    }
    //
    return true;
}

// Apply credential to curl easy handle.
// It returns false if any option fails to set.
// AUTH_BEARER is only fully functional starting with v7.69 (issue #5901)
bool Authentication::apply(CURL* p_curl)
{
    bool ok = true;
    //
    switch ( m_mode )
    {
        case basic:
            ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC );
            ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_USERNAME, m_user.c_str() );
            ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_PASSWORD, m_secret.c_str() );
            break;
        case digest:
            ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST );
            ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_USERNAME, m_user.c_str() );
            ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_PASSWORD, m_secret.c_str() );
            break;
        case bearer:
            ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_HTTPAUTH      , CURLAUTH_BEARER );
            ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_XOAUTH2_BEARER, m_secret.c_str() );
            break;
        case none:
            break;
        default:
            ok = false;
            break;
    }
    //
    return ok;
}

// Reset credential to their default values.
void Authentication::clear(void)
{
    m_mode = none;
    m_user.clear();
    m_secret.clear();
}
