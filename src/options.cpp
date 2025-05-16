#include "common.hpp"
#include "options.hpp"

// Expect a KVCS list of options to set. Example:
//   follow_location=1,insecure=1
// Available keys are:
//   Name              Default  Unit     Comment
//   connect_timeout   30       second   connection timeout   
//   follow_location   false    0 or 1   follow HTTP 3xx redirects
//   insecure          false    0 or 1   disables certificate validation
//   timeout           30       second   receive data timeout
//   verbose           false    0 or 1   debug log on console
bool Options::set(const std::string& p_options)
{
    return parse_cskv( p_options,
                       [ this ]( const std::string & key, const std::string & value )
                       {
                                if ( key == "connect_timeout" ) m_connect_timeout = std::stoi( value );
                           else if ( key == "follow_location" ) m_follow_location = ( value == "1" );
                           else if ( key == "insecure"        ) m_insecure        = ( value == "1" );
                           else if ( key == "timeout"         ) m_timeout         = std::stoi( value );
                           else if ( key == "verbose"         ) m_verbose         = ( value == "1" );
                           // no error on unknown key to ensure forward compatibility
                           //
                           return true;
                       } );
}

// Apply the configured options to the given CURL easy handle.
// It returns false if any option fails to set.
bool Options::apply( CURL * p_curl )
{
    bool ok = true;
    //
    ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_CONNECTTIMEOUT, m_connect_timeout );
    ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_TIMEOUT       , m_timeout );
    //
    // Follow 30x
    ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_FOLLOWLOCATION, m_follow_location ? 1L : 0L );
    //
    // Disable certificate verification
    ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_SSL_VERIFYHOST, m_insecure ? 0L : 1L );
    ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_SSL_VERIFYPEER, m_insecure ? 0L : 1L );
    //
    ok = ok && CURLE_OK == curl_easy_setopt( p_curl, CURLOPT_VERBOSE, m_verbose ? 1L : 0L );
    //
    return ok;
}

// Reset options to their default values
void Options::clear( void )
{
    m_connect_timeout = Options().m_connect_timeout; 
    m_follow_location = Options().m_follow_location; 
    m_insecure        = Options().m_insecure;        
    m_timeout         = Options().m_timeout;         
    m_verbose         = Options().m_verbose;         
}
