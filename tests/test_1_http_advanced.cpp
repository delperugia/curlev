#include <gtest/gtest.h>
#include <iostream>

#include "async.hpp"
#include "common.hpp"
#include "http.hpp"
#include "test_utils.hpp"

TEST( http, headers )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server + "headers" )
                    .add_headers( { { "X-Tst-21", "11" },
                                    { "X-Tst-22", "12" } } )
                    .exec()
                    .get_code();
    //
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.headers.X-Tst-21" ), "11" );
    EXPECT_EQ( json_extract( http->get_body(), "$.headers.X-Tst-22" ), "12" );
    //
    EXPECT_TRUE( http->get_headers().count( "Content-Type" ) == 1 &&
                 http->get_headers().at   ( "Content-Type" ) == "application/json" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server + "response-headers" )
                    .add_query_parameters( { { "X-Tst-31", "41" },
                                             { "X-Tst-32", "42" } } )
                    .exec()
                    .get_code();
    //
    ASSERT_EQ( code, 200 );
    //
    EXPECT_TRUE( http->get_headers().count( "X-Tst-31" ) == 1 &&
                 http->get_headers().at   ( "X-Tst-31" ) == "41" );
    EXPECT_TRUE( http->get_headers().count( "X-Tst-32" ) == 1 &&
                 http->get_headers().at   ( "X-Tst-32" ) == "42" );
  }
  //
  async.stop();
}

TEST( http, post_json )
{
  ASync async;
  async.start();
  //
  {
    const std::string payload = R"({ { "a", "1" }, { "b", "2" } })";
    //
    auto http = HTTP::create( async );
    auto code = http->POST( c_server + "post",
                            "application/json", payload ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 ); 
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.headers.Content-Type" ), "application/json" );
    EXPECT_EQ( json_extract( http->get_body(), "$.data" ), payload );
  }
  //
  {
    const std::string payload = R"({ { "a", "1" }, { "b", "2" } })";
    //
    auto http = HTTP::create( async );
    auto code = http->POST( c_server + "post",
                            "application/json", payload )
                    .add_query_parameters( { { "c", "3" },
                                             { "d", "4" } } ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 2 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 ); 
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.headers.Content-Type" ), "application/json" );
    EXPECT_EQ( json_extract( http->get_body(), "$.data" ), payload );
    EXPECT_EQ( json_extract( http->get_body(), "$.args.c" ), "3" );
    EXPECT_EQ( json_extract( http->get_body(), "$.args.d" ), "4" );
  }
  //
  async.stop();
}

TEST( http, auth )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server + "basic-auth/joe/abc123" )
                    .exec().get_code();
    ASSERT_EQ( code, 401 );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server + "basic-auth/joe/abc123" )
                    .authentication( "mode=basic,user=joe,secret=abc123" )
                    .exec().get_code();
    ASSERT_EQ( code, 200 );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbin + "digest-auth/auth/jim/abc456" )
                    .authentication( "mode=digest,user=jim,secret=abc456" )
                    .exec().get_code();
    ASSERT_EQ( code, 200 );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "bearer/abc789" )
                    .authentication( "mode=bearer,secret=abc789" )
                    .exec().get_code();
    ASSERT_EQ( code, 200 );
  }
  //
  async.stop();
}

TEST( http, clear )
{
  // request, clear, request
  // clear in middle
}

TEST( http, errors )
{
  // 2 requests
  // 0 request
}
