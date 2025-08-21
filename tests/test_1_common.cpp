/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <gtest/gtest.h>

#include "version.hpp"
#include "utils/curl_utils.hpp"
#include "utils/map_utils.hpp"
#include "utils/string_utils.hpp"

using namespace curlev;

//--------------------------------------------------------------------
TEST( common, svtol )
{
  long v;

  // Basic numeric conversions
  EXPECT_TRUE( svtol( "0"        , v ) && v ==         0 );
  EXPECT_TRUE( svtol( "42"       , v ) && v ==        42 );
  EXPECT_TRUE( svtol( "-42"      , v ) && v ==       -42 );
  EXPECT_TRUE( svtol( "123456789", v ) && v == 123456789 );

  // Leading and trailing space are not supported
  EXPECT_FALSE( svtol( ""        , v ) );
  EXPECT_FALSE( svtol( " "       , v ) );
  EXPECT_FALSE( svtol( "\t"      , v ) );
  EXPECT_FALSE( svtol( "  42  "  , v ) );
  EXPECT_FALSE( svtol( "\t-123\n", v ) );

  // Leading zeros
  EXPECT_TRUE( svtol( "000"  , v ) && v ==   0 );
  EXPECT_TRUE( svtol( "0042" , v ) && v ==  42 );
  EXPECT_TRUE( svtol( "-0042", v ) && v == -42 );

  // Invalid input
  EXPECT_FALSE( svtol( "abc"                , v ) );
  EXPECT_FALSE( svtol( "12abc34"            , v ) );
  EXPECT_FALSE( svtol( "abc123"             , v ) );
  EXPECT_FALSE( svtol( "+42"                , v ) );
  EXPECT_FALSE( svtol( "-"                  , v ) );
  EXPECT_FALSE( svtol( "9999999999999999999", v ) );

  // Limits
  if ( sizeof( long ) == 4 )
  {
    EXPECT_TRUE( ! svtol( "2147483648" , v ) );
    EXPECT_TRUE(   svtol( "2147483647" , v ) && v == std::numeric_limits<long>::max() );
    EXPECT_TRUE(   svtol( "-2147483648", v ) && v == std::numeric_limits<long>::min() );
    EXPECT_TRUE( ! svtol( "-2147483649", v ) );
  }
  else
  {
    EXPECT_TRUE( ! svtol( "9223372036854775808" , v ) );
    EXPECT_TRUE(   svtol( "9223372036854775807" , v ) && v == std::numeric_limits<long>::max() );
    EXPECT_TRUE(   svtol( "-9223372036854775808", v ) && v == std::numeric_limits<long>::min() );
    EXPECT_TRUE( ! svtol( "-9223372036854775809", v ) );
  }
}

//--------------------------------------------------------------------
TEST( common, svtoul )
{
  unsigned long v;

  // Basic numeric conversions
  EXPECT_TRUE(  svtoul( "0"        , v ) && v ==         0 );
  EXPECT_TRUE(  svtoul( "42"       , v ) && v ==        42 );
  EXPECT_FALSE( svtoul( "-42"      , v )                   );
  EXPECT_TRUE(  svtoul( "123456789", v ) && v == 123456789 );

  // Leading and trailing space are not supported
  EXPECT_FALSE( svtoul( ""        , v ) );
  EXPECT_FALSE( svtoul( " "       , v ) );
  EXPECT_FALSE( svtoul( "\t"      , v ) );
  EXPECT_FALSE( svtoul( "  42  "  , v ) );
  EXPECT_FALSE( svtoul( "\t-123\n", v ) );

  // Leading zeros
  EXPECT_TRUE(  svtoul( "000"  , v ) && v ==   0 );
  EXPECT_TRUE(  svtoul( "0042" , v ) && v ==  42 );
  EXPECT_FALSE( svtoul( "-0042", v )             );

  // Invalid input
  EXPECT_FALSE( svtoul( "abc"                 , v ) );
  EXPECT_FALSE( svtoul( "12abc34"             , v ) );
  EXPECT_FALSE( svtoul( "abc123"              , v ) );
  EXPECT_FALSE( svtoul( "+42"                 , v ) );
  EXPECT_FALSE( svtoul( "-"                   , v ) );
  EXPECT_FALSE( svtoul( "99999999999999999999", v ) );

  // Limits
  if ( sizeof( long ) == 4 )
  {
    EXPECT_FALSE( svtoul( "4294967296" , v ) );
    EXPECT_TRUE(  svtoul( "4294967295" , v ) && v == std::numeric_limits<unsigned long>::max() );
    EXPECT_TRUE(  svtoul( "0"          , v ) && v == 0                                         );
  }
  else
  {
    EXPECT_FALSE( svtoul( "18446744073709551616" , v ) );
    EXPECT_TRUE(  svtoul( "18446744073709551615" , v ) && v == std::numeric_limits<unsigned long>::max() );
    EXPECT_TRUE(  svtoul( "0"                    , v ) && v == 0                                         );
  }
}

//--------------------------------------------------------------------
TEST( common, trim )
{
  EXPECT_EQ( trim( ""     ), ""   );
  EXPECT_EQ( trim( " "    ), ""   );
  EXPECT_EQ( trim( "  "   ), ""   );
  EXPECT_EQ( trim( " a"   ), "a"  );
  EXPECT_EQ( trim( " ab"  ), "ab" );
  EXPECT_EQ( trim( "  ab" ), "ab" );
  EXPECT_EQ( trim( "a "   ), "a"  );
  EXPECT_EQ( trim( "ab "  ), "ab" );
  EXPECT_EQ( trim( "ab  " ), "ab" );
  EXPECT_EQ( trim( "a"    ), "a"  );
  EXPECT_EQ( trim( "ab"   ), "ab" );
}

//--------------------------------------------------------------------
TEST( common, trim_edge_cases )
{
  EXPECT_EQ( trim( "\t"        ), ""     );
  EXPECT_EQ( trim( "\n"        ), ""     );
  EXPECT_EQ( trim( "\r"        ), ""     );
  EXPECT_EQ( trim( " \t\n\r"   ), ""     );
  EXPECT_EQ( trim( "  a b  "   ), "a b"  );
  EXPECT_EQ( trim( "\ta\t"     ), "a"    );
  EXPECT_EQ( trim( "\na\n"     ), "a"    );
  EXPECT_EQ( trim( " \t ab \n" ), "ab"   );
  EXPECT_EQ( trim( "  a  b  "  ), "a  b" );
  EXPECT_EQ( trim( "a\tb"      ), "a\tb" );
}

//--------------------------------------------------------------------
TEST( common, trim_unicode )
{
  EXPECT_EQ( trim( " \u00A0abc\u00A0 " ), "\u00A0abc\u00A0" ); // non-breaking space is not trimmed
}

//--------------------------------------------------------------------
TEST( common, equal_ascii_ci )
{
  EXPECT_TRUE( equal_ascii_ci( "a", "A" ) );
  EXPECT_TRUE( equal_ascii_ci( "abc", "ABC" ) );
  EXPECT_TRUE( equal_ascii_ci( "AbC", "aBc" ) );
  EXPECT_TRUE( equal_ascii_ci( "test", "TEST" ) );
  EXPECT_TRUE( equal_ascii_ci( "TeSt123", "tEsT123" ) );
  EXPECT_TRUE( equal_ascii_ci( "", "" ) );
  EXPECT_TRUE( equal_ascii_ci( "a", "a" ) );
  EXPECT_TRUE( equal_ascii_ci( "A", "A" ) );
  EXPECT_TRUE( equal_ascii_ci( "123", "123" ) );
  EXPECT_TRUE( equal_ascii_ci( "abcDEF", "ABCdef" ) );

  EXPECT_FALSE( equal_ascii_ci( "abc", "abcd" ) );
  EXPECT_FALSE( equal_ascii_ci( "abc", "ab" ) );
  EXPECT_FALSE( equal_ascii_ci( "abc", "abd" ) );
  EXPECT_FALSE( equal_ascii_ci( "abc", "aBcD" ) );
  EXPECT_FALSE( equal_ascii_ci( "abc", "xyz" ) );
  EXPECT_FALSE( equal_ascii_ci( "abc", "" ) );
  EXPECT_FALSE( equal_ascii_ci( "", "abc" ) );
  EXPECT_FALSE( equal_ascii_ci( "abc1", "abc2" ) );
  EXPECT_FALSE( equal_ascii_ci( "abc!", "abc" ) );

  // Non-ASCII: should not be considered equal
  EXPECT_FALSE( equal_ascii_ci( "abc", "ábć" ) );
  EXPECT_FALSE( equal_ascii_ci( "á", "a" ) );
}

//--------------------------------------------------------------------
TEST( common, parse_cskv )
{
  // Test successful cases
  //
  {
    std::map< std::string, std::string > result;
    //
    bool success = parse_cskv( "key1=value1,key2=value2",
                               [ & ]( std::string_view key, std::string_view value )
                               {
                                 result[ std::string( key ) ] = value;
                                 return true;
                               } );
    //
    EXPECT_TRUE( success );
    EXPECT_EQ( result.size(), 2 );
    EXPECT_EQ( result[ "key1" ], "value1" );
    EXPECT_EQ( result[ "key2" ], "value2" );
  }
  //
  // Test with spaces
  {
    std::map< std::string, std::string > result;
    //
    bool success = parse_cskv( " key1 = value1 , key2 = value2 ",
                               [ & ]( std::string_view key, std::string_view value )
                               {
                                 result[ std::string( key ) ] = value;
                                 return true;
                               } );
    //
    EXPECT_TRUE( success );
    EXPECT_EQ( result.size(), 2 );
    EXPECT_EQ( result[ "key1" ], "value1" );
    EXPECT_EQ( result[ "key2" ], "value2" );
  }
  //
  // Test single key-value pair
  {
    std::map< std::string, std::string > result;
    //
    bool success = parse_cskv( "key=value",
                               [ & ]( std::string_view key, std::string_view value )
                               {
                                 result[ std::string( key ) ] = value;
                                 return true;
                               } );
    //
    EXPECT_TRUE( success );
    EXPECT_EQ( result.size(), 1 );
    EXPECT_EQ( result[ "key" ], "value" );
  }
  //
  // Empty string
  EXPECT_TRUE( parse_cskv( "",
                           []( std::string_view key, std::string_view value ) { return true; } ) );
  //
  // Test failure cases
  //
  // Missing equals sign
  EXPECT_FALSE( parse_cskv( "key1value1",
                            []( std::string_view key, std::string_view value ) { return true; } ) );
  //
  // Invalid format
  EXPECT_FALSE( parse_cskv( "key1=value1,,key2=value2",
                            []( std::string_view key, std::string_view value ) { return true; } ) );
  //
  // Handler rejection
  EXPECT_FALSE( parse_cskv( "key1=value1,key2=value2",
                            []( std::string_view key, std::string_view value ) { return false; } ) );
}

//--------------------------------------------------------------------
TEST( common, curl_slist_checked_append )
{
  curl_slist * slist = nullptr;
  //
  // Append to empty list
  EXPECT_TRUE( curl_slist_checked_append( slist, "Header1: value1" ) );
  ASSERT_NE( slist, nullptr );
  EXPECT_STREQ( slist->data, "Header1: value1" );
  //
  // Append another header
  EXPECT_TRUE( curl_slist_checked_append( slist, "Header2: value2" ) );
  ASSERT_NE( slist, nullptr );
  ASSERT_NE( slist->next, nullptr );
  EXPECT_STREQ( slist->next->data, "Header2: value2" );
  //
  // Append an empty string (should not change list)
  curl_slist * slist_before = slist;
  EXPECT_FALSE( curl_slist_checked_append( slist, "" ) );
  ASSERT_NE( slist, nullptr );
  ASSERT_NE( slist->next, nullptr );
  EXPECT_EQ( slist_before, slist );
  EXPECT_STREQ( slist->next->data, "Header2: value2" );
  //
  curl_slist_free_all( slist );
}

//--------------------------------------------------------------------

std::string enc( std::string r, const key_values & p, char f, char s )
{
  append_url_encoded( r, p, f, s );
  return r;
}

TEST( common, url_encode )
{
  EXPECT_EQ( enc( "", {}            , 0, '&'), ""    );
  EXPECT_EQ( enc( "", {{ "a", "1" }}, 0, '&'), "a=1" );
  EXPECT_EQ( enc( "", {{ "a", "1" },
                       { "b", "2" }}, 0, '&'), "b=2&a=1" );
  //
  EXPECT_EQ( enc( "url", {}            , '?', '&'), "url"    );
  EXPECT_EQ( enc( "url", {{ "a", "1" }}, '?', '&'), "url?a=1" );
  EXPECT_EQ( enc( "url", {{ "a", "1" },
                          { "b", "2" }}, '?', '&'), "url?b=2&a=1" );
  //
  EXPECT_EQ( enc( "url?c=3", {}            , '&', '&'), "url?c=3"    );
  EXPECT_EQ( enc( "url?c=3", {{ "a", "1" }}, '&', '&'), "url?c=3&a=1" );
  EXPECT_EQ( enc( "url?c=3", {{ "a", "1" },
                              { "b", "2" }}, '&', '&'), "url?c=3&b=2&a=1" );
  //
  EXPECT_EQ( enc( "", {{ "a", "abc123"             }}, 0, 0), "a=abc123");
  EXPECT_EQ( enc( "", {{ "a", "MixedCASE123!@#"    }}, 0, 0), "a=MixedCASE123%21%40%23");
  EXPECT_EQ( enc( "", {{ "a", "hello world"        }}, 0, 0), "a=hello%20world");
  EXPECT_EQ( enc( "", {{ "a", ":/?#[]@!$&'()*+,;=" }}, 0, 0), "a=%3A%2F%3F%23%5B%5D%40%21%24%26%27%28%29%2A%2B%2C%3B%3D");
  EXPECT_EQ( enc( "", {{ "a", "<>%{}|\\^~[]`"      }}, 0, 0), "a=%3C%3E%25%7B%7D%7C%5C%5E~%5B%5D%60");
  EXPECT_EQ( enc( "", {{ "a", "é"                  }}, 0, 0), "a=%C3%A9");
  EXPECT_EQ( enc( "", {{ "a", "ü"                  }}, 0, 0), "a=%C3%BC");
  EXPECT_EQ( enc( "", {{ "a", "漢"                 }}, 0, 0), "a=%E6%BC%A2");
  EXPECT_EQ( enc( "", {{ "a", "C++ > Java"         }}, 0, 0), "a=C%2B%2B%20%3E%20Java");
  EXPECT_EQ( enc( "", {{ "a", "already%20encoded"  }}, 0, 0), "a=already%2520encoded");
  //
  EXPECT_EQ( enc( "", {{ "abc123"            , "x" }}, 0, 0), "abc123=x");
  EXPECT_EQ( enc( "", {{ "MixedCASE123!@#"   , "x" }}, 0, 0), "MixedCASE123%21%40%23=x");
  EXPECT_EQ( enc( "", {{ "hello world"       , "x" }}, 0, 0), "hello%20world=x");
  EXPECT_EQ( enc( "", {{ ":/?#[]@!$&'()*+,;=", "x" }}, 0, 0), "%3A%2F%3F%23%5B%5D%40%21%24%26%27%28%29%2A%2B%2C%3B%3D=x");
  EXPECT_EQ( enc( "", {{ "<>%{}|\\^~[]`"     , "x" }}, 0, 0), "%3C%3E%25%7B%7D%7C%5C%5E~%5B%5D%60=x");
  EXPECT_EQ( enc( "", {{ "é"                 , "x" }}, 0, 0), "%C3%A9=x");
  EXPECT_EQ( enc( "", {{ "ü"                 , "x" }}, 0, 0), "%C3%BC=x");
  EXPECT_EQ( enc( "", {{ "漢"                , "x" }}, 0, 0), "%E6%BC%A2=x");
  EXPECT_EQ( enc( "", {{ "C++ > Java"        , "x" }}, 0, 0), "C%2B%2B%20%3E%20Java=x");
  EXPECT_EQ( enc( "", {{ "already%20encoded" , "x" }}, 0, 0), "already%2520encoded=x");
}
