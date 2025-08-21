/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <gtest/gtest.h>
#include <iostream>

#include "async.hpp"
#include "http.hpp"
#include "test_utils.hpp"

using namespace curlev;

//--------------------------------------------------------------------
TEST( http_basic, async )
{
  {
    ASync async;
    async.start();
    async.stop();
  }
  //
  {
    ASync async;
    async.start();
    async.start();
    async.stop();
  }
  //
  {
    ASync async;
    async.start();
    {
      auto http = HTTP::create( async );
    }
    async.stop();
  }
  //
  {
    ASync async;
    async.start();
    //
    {
      auto http = HTTP::create( async );
      auto code = http->GET( c_server_httpbun + "get" ).exec().get_code();
      //
      EXPECT_EQ( code, 200 );
    }
    //
    async.stop();
    async.stop();
  }
  //
  {
    ASync async1;
    ASync async2;
    ASync async3;
    async1.start();
    async1.start();
    async2.start();
    async1.stop();
    async3.start();
    async2.stop();
    async3.stop();
    async3.stop();
  }
}

//--------------------------------------------------------------------
// Ensure that all methods are properly implemented. Once this is done it is possible
// to focus on the various function signatures of GET and POST.

// Internal libcurl constant
#ifndef CURL_MAX_INPUT_LENGTH
#define CURL_MAX_INPUT_LENGTH 8'000'000
#endif

TEST( http_basic, cskv_error )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    long code;
    //
    code = http->GET( c_server_httpbun + "get" ).options("").authentication("").exec().get_code();
    EXPECT_EQ( code, 200 );
    //
    code = http->GET( c_server_httpbun + "get" ).options( "alpha" ).exec().get_code(); // unknown
    EXPECT_EQ( code, c_error_options_format );
    //
    code = http->GET( c_server_httpbun + "get" ).options( "maxredirs" ).exec().get_code(); // no =
    EXPECT_EQ( code, c_error_options_format );
    //
    code = http->GET( c_server_httpbun + "get" ).options( "maxredirs=" ).exec().get_code(); // no value
    EXPECT_EQ( code, c_error_options_format );
    //
    code = http->GET( c_server_httpbun + "get" ).options( "maxredirs=x" ).exec().get_code(); // invalid value
    EXPECT_EQ( code, c_error_options_format );
    //
    code = http->GET( c_server_httpbun + "get" ).authentication( "beta" ).exec().get_code(); // unknown
    EXPECT_EQ( code, c_error_authentication_format );
    //
    code = http->GET( c_server_httpbun + "get" ).authentication( "mode=x" ).exec().get_code(); // unknown mode
    EXPECT_EQ( code, c_error_authentication_format );
    //
    code = http->GET( c_server_httpbun + "get" ).options( "maxredirs=-5" ).exec().get_code();
    EXPECT_EQ( code, c_error_options_set );
    //
    #if LIBCURL_VERSION_NUM >= CURL_VERSION_BITS( 7, 65, 0 )
    std::string too_long = std::string( CURL_MAX_INPUT_LENGTH + 16, '*' );
    code = http->GET( c_server_httpbun + "get" ).authentication( "mode=basic,user=joe,secret="+too_long ).exec().get_code();
    EXPECT_EQ( code, c_error_authentication_set );
    #endif
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
// Ensure that all methods are properly implemented. Once this is done it is possible
// to focus on the various function signatures of GET and POST.
TEST( http_basic, method_equivalence )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->DELETE( c_server_httpbun + "delete" ).exec().get_code();
    //
    EXPECT_EQ( code, 200 );
  }
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "get" ).exec().get_code();
    //
    EXPECT_EQ( code, 200 );
  }
  {
    auto http = HTTP::create( async );
    auto code = http->PATCH( c_server_httpbun + "patch" ).exec().get_code();
    //
    EXPECT_EQ( code, 200 );
  }
  {
    auto http = HTTP::create( async );
    auto code = http->POST( c_server_httpbun + "post" ).exec().get_code();
    //
    EXPECT_EQ( code, 200 );
  }
  {
    auto http = HTTP::create( async );
    auto code = http->PUT( c_server_httpbun + "put" ).exec().get_code();
    //
    EXPECT_EQ( code, 200 );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
// Validate the p_query_parameters handling in requests without body
TEST( http_basic, get )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "get" ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( http->get_content_type(), "application/json" );
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "get", { { "a", "11" } } ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.args.a"), "11" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "get", { { "bb", "23" }, { "a", "21" } } ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 2 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.args.a"  ), "21" );
    EXPECT_EQ( json_extract( http->get_body(), "$.args.bb" ), "23" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "get?ax=31", { { "bx", "32" } } ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 2 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.args.ax"), "31" );
    EXPECT_EQ( json_extract( http->get_body(), "$.args.bx"), "32" );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
TEST( http_basic, post )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->POST( c_server_httpbun + "post" ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->POST( c_server_httpbun + "post" )
                    .set_parameters( { { "a", "1" }, { "b", "2" } } ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 2 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.form.a" ), "1" );
    EXPECT_EQ( json_extract( http->get_body(), "$.form.b" ), "2" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->POST( c_server_httpbun + "post", { { "a", "1" }, { "b", "2" } } )
                    .exec()
                    .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 2 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.args.a" ), "1"  );
    EXPECT_EQ( json_extract( http->get_body(), "$.args.b" ), "2" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->POST( c_server_httpbun + "post", { { "b", "2" } } )
                    .set_parameters( { { "a", "1" } } )
                    .exec()
                    .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.form.a" ), "1" );
    EXPECT_EQ( json_extract( http->get_body(), "$.args.b" ), "2" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->POST( c_server_httpbun + "post", { { "q1", "2" } } )
                    .set_parameters ( { { "b1", "1" },
                                        { "b2", "3" } } )
                    .exec()
                    .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 2 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.form.b1" ), "1" );
    EXPECT_EQ( json_extract( http->get_body(), "$.form.b2" ), "3" );
    EXPECT_EQ( json_extract( http->get_body(), "$.args.q1" ), "2" );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
TEST( http_basic, put_patch )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->PATCH( c_server_httpbun + "patch" )
                    .set_parameters( { { "a", "a1" }, { "b", "a2" } } ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 2 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.form.a" ), "a1" );
    EXPECT_EQ( json_extract( http->get_body(), "$.form.b" ), "a2" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->PUT( c_server_httpbun + "put" )
                    .set_parameters( { { "a", "u1" } } ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.form.a" ), "u1" );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
#ifndef PROJECT_ROOT_DIR
  #define PROJECT_ROOT_DIR "tests/"
#endif

TEST( http_basic, post_mime )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->POST( c_server_httpbun + "post" )
            .set_mime( { mime::parameter{ "m1", "40" } } )
            .exec()
            .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.form.m1" ), "40" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->POST( c_server_httpbun + "post", { { "q4", "44" } } )
            .set_mime( { mime::parameter{ "m2", "42" },
                         mime::parameter{ "m3", "43" } } )
            .exec()
            .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 2 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.args.q4" ), "44" );
    EXPECT_EQ( json_extract( http->get_body(), "$.form.m2" ), "42" );
    EXPECT_EQ( json_extract( http->get_body(), "$.form.m3" ), "43" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->POST( c_server_httpbun + "post" )
            .set_mime( { mime::data{ "f1", "Hello!", "text/plain", "f1.txt" } } )
            .exec()
            .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 1 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.files.f1.content"              ), "Hello!" );
    EXPECT_EQ( json_extract( http->get_body(), "$.files.f1.filename"             ), "f1.txt" );
    EXPECT_EQ( json_extract( http->get_body(), "$.files.f1.headers.Content-Type" ), "text/plain" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->POST( c_server_httpbun + "post", { { "q23", "33" } } )
            .set_mime( { mime::parameter{ "m21", "51" },
                         mime::parameter{ "m22", "52" },
                         mime::file     { "f21", PROJECT_ROOT_DIR "/tests/data.txt", "text/html", "f21.txt" } } )
            .exec()
            .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 1 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 2 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 1 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.args.q23" ), "33" );
    EXPECT_EQ( json_extract( http->get_body(), "$.form.m21" ), "51" );
    EXPECT_EQ( json_extract( http->get_body(), "$.form.m22" ), "52" );
    EXPECT_EQ( json_extract( http->get_body(), "$.files.f21.content"              ), "abc123" );
    EXPECT_EQ( json_extract( http->get_body(), "$.files.f21.filename"             ), "f21.txt" );
    EXPECT_EQ( json_extract( http->get_body(), "$.files.f21.headers.Content-Type" ), "text/html" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->POST( c_server_httpbun + "payload" )
            .set_mime( { mime::alternatives{
                           mime::data{ "", "text", "text/plain", ""},
                           mime::data{ "", "html", "text/html" , ""}, },
                         mime::file     { "f21", PROJECT_ROOT_DIR "/tests/data.txt", "text/html", "f21.txt" } } )
            .exec()
            .get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_TRUE( http->get_body().find( "multipart/alternative" ) != std::string::npos );
    EXPECT_TRUE( http->get_body().find( "text"                  ) != std::string::npos );
    EXPECT_TRUE( http->get_body().find( "html"                  ) != std::string::npos );
    EXPECT_TRUE( http->get_body().find( "abc123"                ) != std::string::npos );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
// Validate the p_query_parameters handling in requests without body
TEST( http_basic, launch )
{
  ASync async;
  async.start();
  //
  {
    auto http   = HTTP::create( async );
    auto future = http->GET( c_server_httpbun + "get", { { "a", "11" } } ).launch();
    //
    auto response = future.get();
    //
    ASSERT_EQ( response.code, 200 );
    EXPECT_EQ( json_extract( response.body, "$.args.a" ), "11" );
    EXPECT_TRUE( http->get_body().empty() ); // since it was moved to the std::future
  }
  //
  {
    auto future1 =
        HTTP::create( async )->GET( c_server_httpbun + "get", { { "a", "21" } } )
                              .launch();
    auto future2 =
        HTTP::create( async )->GET( c_server_httpbun + "get", { { "a", "22" } } )
                              .launch();
    //
    auto response2 = future2.get();
    auto response1 = future1.get();
    //
    EXPECT_EQ( response1.code, 200 );
    EXPECT_EQ( json_extract( response1.body, "$.args.a" ), "21" );
    nlohmann::json json1;
    EXPECT_TRUE( response1.get_json( json1 ) && json1[ "args" ][ "a" ] == "21" );
    //
    EXPECT_EQ( response2.code, 200 );
    EXPECT_EQ( json_extract( response2.body, "$.args.a" ), "22" );
#if __has_include( <rapidjson/document.h> )
    rapidjson::Document json2;
    EXPECT_TRUE( response2.get_json( json2 ) && json2[ "args" ][ "a" ] == "22" );
#endif
  }
  //
  async.stop();
}
