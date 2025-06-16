/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include "certificates.hpp"
#include "common.hpp"

namespace curlev
{

namespace
{

  // The libcurl options associated to our keys
  static const std::map< std::string, CURLoption >
    s_keys_to_options = {
      { "engine"            , CURLOPT_SSLENGINE         },          
      { "sslcert"           , CURLOPT_SSLCERT           },	         
      { "sslcerttype"       , CURLOPT_SSLCERTTYPE       },          
      { "sslkey"            , CURLOPT_SSLKEY            },	         
      { "sslkeytype"        , CURLOPT_SSLKEYTYPE        },           
      { "keypasswd"         , CURLOPT_KEYPASSWD         },  	       
      { "cainfo"            , CURLOPT_CAINFO            },
      { "capath"            , CURLOPT_CAPATH            },	       
      { "proxy_sslcert"     , CURLOPT_PROXY_SSLCERT     },      	   
      { "proxy_sslcerttype" , CURLOPT_PROXY_SSLCERTTYPE },          
      { "proxy_sslkey"      , CURLOPT_PROXY_SSLKEY      },      	   
      { "proxy_sslkeytype"  , CURLOPT_PROXY_SSLKEYTYPE  },           
      { "proxy_keypasswd"   , CURLOPT_PROXY_KEYPASSWD   },        	 
      { "proxy_cainfo"      , CURLOPT_PROXY_CAINFO      },      	  
      { "proxy_capath"      , CURLOPT_PROXY_CAPATH      },      	
    };

}; // namespace

//--------------------------------------------------------------------
// Expect a CSKV list of credential details. Example:
//   sslcert=client.pem,sslkey=key.pem,keypasswd=s3cret
bool Certificates::set( const std::string & p_options )
{
  return parse_cskv(
    p_options,
    [ this ]( const std::string & key, const std::string & value )
    {
      // no error on unknown key to ensure forward compatibility
      m_parameters[ key ] = value;
      return true;
    } );
}

//--------------------------------------------------------------------
// Apply credential to curl easy handle.
// It returns false if any option fails to set.
// AUTH_BEARER is only fully functional starting with v7.69 (issue #5901).
bool Certificates::apply( CURL * p_curl )
{
  bool ok = true;
  //
  // Set all supported options to either the configured value or default
  for ( const auto & [ key, option ] : s_keys_to_options )
  {
    if ( const auto iter = m_parameters.find( key ); iter == m_parameters.end() || iter->second.empty() )
      ok = ok && easy_setopt( p_curl, option, nullptr );              // not configured or set to empty: default value
    else
      ok = ok && easy_setopt( p_curl, option, iter->second.c_str() ); // configured: set value
  }
  //
  return ok;
}

//--------------------------------------------------------------------
// Reset credential to its default value
// libcurl doesn't reset the CA keys to their default values when setting nullptr,
// so we always restore the values captured during startup (by ASync)
void Certificates::set_default( const std::string & p_ca_info, const std::string & p_ca_path )
{
  m_parameters.clear();
  //
  m_parameters[ "cainfo" ] = m_parameters[ "proxy_cainfo" ] = p_ca_info;
  m_parameters[ "capath" ] = m_parameters[ "proxy_capath" ] = p_ca_path;
}

} // namespace curlev
