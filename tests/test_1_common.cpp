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
// cppcheck-suppress-begin nullPointer
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
// cppcheck-suppress-end nullPointer

//--------------------------------------------------------------------
TEST( common, parse_cskv )
{
  // Test successful cases
  //
  {
    std::map< std::string, std::string > result;
    //
    bool success = parse_cskv( "key1=value1,key2=value2",
                               [ & ]( const std::string & key, const std::string & value )
                               {
                                 result[ key ] = value;
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
                               [ & ]( const std::string & key, const std::string & value )
                               {
                                 result[ key ] = value;
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
                               [ & ]( const std::string & key, const std::string & value )
                               {
                                 result[ key ] = value;
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
                           []( const std::string &, const std::string & ) { return true; } ) );
  //
  // Test failure cases
  //
  // Missing equals sign
  EXPECT_FALSE( parse_cskv( "key1value1",
                            []( const std::string &, const std::string & ) { return true; } ) );
  //                              
  // Invalid format
  EXPECT_FALSE( parse_cskv( "key1=value1,,key2=value2",
                            []( const std::string &, const std::string & ) { return true; } ) );
  //
  // Handler rejection
  EXPECT_FALSE( parse_cskv( "key1=value1,key2=value2",
                            []( const std::string &, const std::string & ) { return false; } ) );
}
