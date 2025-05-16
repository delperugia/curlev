#include <gtest/gtest.h>
#include "common.hpp"

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

TEST( common, trim_unicode )
{
    EXPECT_EQ( trim( " \u00A0abc\u00A0 " ), "\u00A0abc\u00A0" ); // non-breaking space is not trimmed
}

#include <curl/curl.h>

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
    EXPECT_STREQ( slist->next->data, "Header2: value2" );
    //
    // Append an empty string (should not change list)
    curl_slist * slist_before = slist;
    EXPECT_FALSE( curl_slist_checked_append( slist, "" ) );
    EXPECT_EQ( slist_before, slist );
    EXPECT_STREQ( slist->next->data, "Header2: value2" );
    //
    curl_slist_free_all( slist );
}
