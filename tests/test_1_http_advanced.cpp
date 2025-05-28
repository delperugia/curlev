/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <gtest/gtest.h>
#include <iostream>

#include "async.hpp"
#include "common.hpp"
#include "http.hpp"
#include "test_utils.hpp"

using namespace curlev;

//--------------------------------------------------------------------
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

//--------------------------------------------------------------------
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

//--------------------------------------------------------------------
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
    auto code = http->GET( c_server + "digest-auth/auth/jim/abc456" )
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

//--------------------------------------------------------------------
// Various clear in middle of requests
TEST( http, clear )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    //
    auto code = http->GET( c_server + "get", { { "a1", "11" }, { "a2", "12" } } ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 2 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 ); 
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.args.a1" ), "11" );
    EXPECT_EQ( json_extract( http->get_body(), "$.args.a2" ), "12" );
    //
    http->clear();
    //
    code = http->POST( c_server + "post", { { "c1", "21" } } )
               .add_query_parameters( { { "c2", "22" } } )
               .exec()
               .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 ); 
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.form.c1" ), "21" );
    EXPECT_EQ( json_extract( http->get_body(), "$.args.c2" ), "22" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server + "basic-auth/joe/abc123" )
                    .authentication( "mode=basic,user=joe,secret=abc123" )
                    .exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    http->clear();
    //
    code = http->GET( c_server + "basic-auth/joe/abc123" )
                    .exec().get_code();
    EXPECT_EQ( code, 401 );
  }
  //
  {
    auto http = HTTP::create( async );
    //
    auto code = http->POST( c_server + "post", { { "c3", "31" } } )
               .add_query_parameters( { { "c4", "32" } } )
               .clear()
               .GET( c_server + "get", { { "d1", "41" } } )
               .exec()
               .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 ); 
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.args.d1" ), "41" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->POST( c_server + "post", { { "c5", "51" } } )
            .start()
            .clear()
            .join()
            .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.form.c5" ), "51" );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
// 2nd request attempted while the 1st is running
TEST( http, errors )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->GET( c_server + "delay/1" )
            .start()
              .GET( c_server + "invalid", { { "a", "11" } } )
              .exec()
            .join()
            .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( http->get_body(), "OK" );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
TEST( http, user_cb )
{
  ASync async;
  async.start();
  //
  {
    auto http    = HTTP::create( async );
    long cb_code = 0;
    std::string cb_body;
    auto        code =
        http->GET( c_server + "delay/1" )
            .start(
                [ &cb_code, &cb_body ]( const auto & h )
                {
                  cb_code = h.get_code();
                  cb_body = h.get_body();
                } )
            .join()
            .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( cb_code, code );
    EXPECT_EQ( cb_body, http->get_body() );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
TEST( http, consecutive )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->POST( c_server + "payload", "plain/text", "123" ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    code = http->POST( c_server + "payload", "plain/text", "456" ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( http->get_body(), "456" );
  }
  //
  async.stop();
}