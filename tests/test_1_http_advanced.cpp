/*********************************************************************
 * Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************/

#include <gtest/gtest.h>
#include <iostream>

#if __has_include( <rapidjson/document.h> )
  #include <rapidjson/writer.h>
  #include <rapidjson/stringbuffer.h>
#endif

#include "async.hpp"
#include "http.hpp"
#include "test_utils.hpp"

using namespace curlev;

//--------------------------------------------------------------------
TEST( http_advanced, request )
{
  ASync async;
  async.start();
  //
  for ( const auto & [ document, verb ] :
        key_values{ { "get", "GET" }, { "delete", "DELETE" }, { "post", "POST" }, { "put", "PUT" }, { "patch", "PATCH" } } )
  {
    nlohmann::json json;
    //
    auto http = HTTP::create( async );
    auto code = http->REQUEST( verb, c_server_httpbun + document ).exec().get_code();
    ASSERT_EQ( code, 200 );
    ASSERT_TRUE( http->get_json( json ) );
    //
    EXPECT_EQ( json["method"]      , verb );
    EXPECT_EQ( json["args" ].size(), 0    );
    EXPECT_EQ( json["form" ].size(), 0    );
    EXPECT_EQ( json["files"].size(), 0    );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
TEST( http_advanced, results )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->exec().get_code();
    //
    EXPECT_EQ( code, CURLE_URL_MALFORMAT );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->GET( c_server_httpbun + "delay/2" )
            .options( "timeout=500" )
            .exec()
            .get_code();
    //
    EXPECT_EQ( code, CURLE_OPERATION_TIMEDOUT );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->GET( "http://localhost:9999/" )
            .exec()
            .get_code();
    //
    EXPECT_EQ( code, CURLE_COULDNT_CONNECT );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->GET( "http://server.that.doesnt.exist.gouv/" )
            .exec()
            .get_code();
    //
    EXPECT_EQ( code, CURLE_COULDNT_RESOLVE_HOST );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
TEST( http_advanced, headers )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "headers" )
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
    auto code = http->GET( c_server_httpbun + "response-headers", { { "X-Tst-31", "41" },
                                                                   { "X-Tst-32", "42" } } )
                    .exec()
                    .get_code();
    //
    ASSERT_EQ( code, 200 );
    //
    EXPECT_TRUE( http->get_headers().count( "X-Tst-31" ) == 1    &&
                 http->get_headers().at   ( "X-Tst-31" ) == "41" );
    EXPECT_TRUE( http->get_headers().count( "X-Tst-32" ) == 1    &&
                 http->get_headers().at   ( "X-Tst-32" ) == "42" &&
                 http->get_headers().count( "x-tst-32" ) == 1    &&
                 http->get_headers().at   ( "x-tst-32" ) == "42" &&
                 http->get_headers().count( "X-TST-32" ) == 1    &&
                 http->get_headers().at   ( "X-TST-32" ) == "42" );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
TEST( http_advanced, post_json )
{
  ASync async;
  async.start();
  //
  {
    const std::string payload = R"({ { "a", "1" }, { "b", "2" } })";
    //
    auto http = HTTP::create( async );
    auto code = http->POST( c_server_httpbun + "post" )
                          .set_body( "application/json", std::string( payload ) )
                          .exec().get_code();
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
    auto code = http->POST( c_server_httpbun + "post", { { "c", "3" },
                                                         { "d", "4" } } )
                          .set_body( "application/json", std::string( payload ) )
                          .exec().get_code();
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
TEST( http_advanced, rest )
{
  ASync async;
  async.start();
  //
  for ( const auto & [ document, verb ] :
        key_values{ { "post", "POST" }, { "put", "PUT" }, { "patch", "PATCH" } } )
  {
    nlohmann::json payload = nlohmann::json::parse( R"({ "a": "1", "b": "2" })" );
    nlohmann::json json;
    //
    auto http = HTTP::create( async );
    auto code = http->REST( c_server_httpbun + document, verb, payload, { { "c", "3" } } )
                     .exec().get_code();
    ASSERT_EQ( code, 200 );
    ASSERT_TRUE( http->get_json( json ) );
    //
    EXPECT_EQ( json["method"]                 , verb               );
    EXPECT_EQ( json["headers"]["Content-Type"], "application/json" );
    EXPECT_EQ( json["json"]["a"]              , "1"                );
    EXPECT_EQ( json["json"]["b"]              , "2"                );
    EXPECT_EQ( json["args"]["c"]              , "3"                );
  }
  //
#if __has_include( <rapidjson/document.h> )
  {
    rapidjson::Document payload, json;
    payload.Parse( R"({ "a": "1", "b": "2" })" );
    //
    auto http = HTTP::create( async );
    auto code = http->REST( c_server_httpbun + "post", "POST", payload, { { "c", "3" } } )
                     .exec().get_code();
    ASSERT_EQ( code, 200 );
    ASSERT_TRUE( http->get_json( json ) );
    //
    EXPECT_EQ( json["headers"]["Content-Type"], "application/json" );
    EXPECT_EQ( json["json"]["a"]              , "1"                );
    EXPECT_EQ( json["json"]["b"]              , "2"                );
    EXPECT_EQ( json["args"]["c"]              , "3"                );
  }
#endif
  //
  async.stop();
}

//--------------------------------------------------------------------
// Memory leak with mode=bearer with libcurl<7.84.0 (issues #8841)
TEST( http_advanced, auth )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "basic-auth/joe/abc123" )
                    .exec().get_code();
    ASSERT_EQ( code, 401 );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "basic-auth/joe/abc123" )
                    .authentication( "mode=basic,user=joe,secret=abc123" )
                    .exec().get_code();
    ASSERT_EQ( code, 200 );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "digest-auth/auth/jim/abc456" )
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
TEST( http_advanced, certificates )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_certificates )
                    .certificates( "sslcert=client.pem,sslkey=key.pem,keypasswd=s3cret" )
                    .exec().get_code();
    EXPECT_EQ( code, CURLE_SSL_CERTPROBLEM );
    //
    code = http->GET( c_server_certificates )
                    .exec().get_code();
    EXPECT_EQ( code, 200 );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
// 2nd request attempted while the 1st is running
TEST( http_advanced, while_running )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->GET( c_server_httpbun + "delay/0" )
            .start()
              .GET( c_server_httpbun + "invalid", { { "a", "11" } } )
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
TEST( http_advanced, user_cb )
{
  ASync async;
  async.start();
  //
  {
    std::string cb_body;
    auto        http    = HTTP::create( async );
    long        cb_code = 0;
    auto        code =
        http->GET( c_server_httpbun + "get" )
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
  {
    auto http = HTTP::create( async );
    auto code =
        http->GET( c_server_httpbun + "get" )
            .start(
                []( const auto & h )
                {
                  throw std::runtime_error("error");
                } )
            .join()
            .get_code();
    ASSERT_EQ( code, c_error_user_callback );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
TEST( http_advanced, consecutive )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->POST( c_server_httpbun + "payload" )
                    .set_body( "plain/text", "123" )
                    .exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    code = http->POST( c_server_httpbun + "payload" )
                    .set_body( "plain/text", "456" )
                    .exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( http->get_body(), "456" );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
TEST( http_advanced, compression )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_compress )
                    .options( "accept_compression=0" )
                    .exec()
                    .get_code();
    //
    ASSERT_EQ( code, 200 );
    //
    EXPECT_TRUE( http->get_headers().count( "content-encoding" ) == 0 );
    EXPECT_GT( http->get_body().size(), 1024 ); // counter test for http_complex.max_size
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_compress )
                    .options( "accept_compression=1" )
                    .exec()
                    .get_code();
    //
    ASSERT_EQ( code, 200 );
    //
    EXPECT_TRUE( http->get_headers().count( "content-encoding" ) > 0 );
    EXPECT_GT( http->get_body().size(), 1024 ); // counter test for http_complex.max_size
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
TEST( http_advanced, default_options )
{
  ASync async;
  async.start();
  async.options( "follow_location=1" );
  async.authentication( "mode=basic,user=joe,secret=abc123" );
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "redirect/1" )
                    .exec()
                    .get_code();
    EXPECT_EQ( code, 200 );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "basic-auth/joe/abc123" )
                    .exec()
                    .get_code();
    ASSERT_EQ( code, 200 );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->GET( c_server_httpbun + "redirect/1" )
            .options( "follow_location=0" )
            .exec()
            .get_code();
    EXPECT_EQ( code, 302 );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->GET( c_server_httpbun + "basic-auth/joe/abc123" )
            .authentication( "mode=basic,user=joe,secret=bad" )
            .exec()
            .get_code();
    ASSERT_EQ( code, 401 );
  }
  //
  async.stop();
}
