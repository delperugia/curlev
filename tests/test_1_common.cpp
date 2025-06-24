/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <gtest/gtest.h>

#include "common.hpp"

using namespace curlev;

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
  EXPECT_TRUE( svtol( "2147483647" , v ) && v ==  2147483647 );
  EXPECT_TRUE( svtol( "-2147483647", v ) && v == -2147483647 );
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
