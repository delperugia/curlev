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
// 9 simultaneous requests
TEST( http_complex, simultaneous )
{
  ASync async;
  async.start();
  //
  {
    std::vector< std::shared_ptr< HTTP > > https;
    std::vector< std::string >             codes =
      { "200", "204", "302", "400", "401", "404", "409", "501", "503" };
    //
    for ( const std::string & expected_code : codes )
    {
      https.emplace_back( HTTP::create( async ) );
      https.back()->GET( c_server_httpbun + "status/" + expected_code ).start();
    }
    //
    for ( auto i = 0; i < codes.size(); i++ )
      EXPECT_EQ( std::to_string( https[ i ]->join().get_code() ), codes[ i ] );
    //
    EXPECT_EQ( async.peak_requests(), codes.size() );
    EXPECT_EQ( async.active_requests(), 0 );
    EXPECT_FALSE( async.protocol_crashed() );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
// Request continues and ASync is stopped
TEST( http_complex, detached )
{
  bool done = false;
  //
  {
    ASync async;
    async.start();
    //
    {
      HTTP::create( async )
          ->GET( c_server_httpbun + "delay/1" )
          .start(
              [ &done ]( const auto & http )
              {
                ASSERT_EQ( http.get_code(), 200 );
                EXPECT_EQ( http.get_body(), "OK" );
                done = true;
              } );
    }
    //
    async.stop(); // should wait for the end of all pending requests
  }
  //
  EXPECT_TRUE( done );
}

//--------------------------------------------------------------------
// Check that cookies if activated are properly handled
// and don't spill on other handles.
TEST( http_complex, cookies )
{
  ASync async;
  async.start();
  //
  {
    auto http1 = HTTP::create( async );
    auto http2 = HTTP::create( async );
    //
    // Set a cookie
    auto code = http1->GET( c_server_httpbun + "cookies/set", { { "d1", "61" } } ).options ( "cookies=1" ).exec().get_code();
    ASSERT_EQ( code, 302 );
    //
    // And retrieve it
    code = http1->GET( c_server_httpbun + "cookies" ).options ( "cookies=1" ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_extract( http1->get_body(), "$.cookies.d1" ), "61" );
    //
    // But must not be retrieved from another connection
    code = http2->GET( c_server_httpbun + "cookies" ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http2->get_body(), "$.cookies" ), 0 );
  }
  //
  {
    // The previous cookie must be retrieve from a new context
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "cookies" ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.cookies" ), 0 );
  }
  //
  {
    auto http1 = HTTP::create( async );
    auto http2 = HTTP::create( async );
    //
    // Set cookie
    auto code = http1->GET( c_server_httpbun + "cookies/set", { { "e1", "71" } } ).options ( "cookies=1" ).exec().get_code();
    ASSERT_EQ( code, 302 );
    //
    // Set another cookie
    code = http2->GET( c_server_httpbun + "cookies/set", { { "e2", "72" } } ) .options ( "cookies=1" ) .exec().get_code();
    ASSERT_EQ( code, 302 );
    //
    // Retrieve 2nd cookie
    code = http2->GET( c_server_httpbun + "cookies" ) .options ( "cookies=1" ) .exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_extract( http2->get_body(), "$.cookies.e2" ), "72" );
    //
    // Retrieve 1st cookie
    code = http1->GET( c_server_httpbun + "cookies" ) .options ( "cookies=1" ) .exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_extract( http1->get_body(), "$.cookies.e1" ), "71" );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
// Validate the p_query_parameters handling in requests without body
TEST( http_complex, redirect )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server_httpbun + "redirect", { { "url", "http://somewhere.com/" } } )
                     .exec().get_code();
    ASSERT_EQ( code, 302 );
    //
    EXPECT_EQ(  http->get_redirect_url(), "http://somewhere.com/" );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
// A 308 to the target site is received from httpbun.com
TEST( http_complex, proxy )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( "http://example.com/" )
                     .options( "proxy=" + c_server_httpbun )
                     .exec().get_code();
    ASSERT_EQ( code, 308 );
    //
    EXPECT_EQ(  http->get_redirect_url(), "https://example.com/" );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
// Abort the request then retry
TEST( http_complex, abort )
{
  std::atomic_int cb_count = 0;
  //
  {
    ASync async;
    async.start();
    //
    {
      auto http = HTTP::create( async );
      //
      http->GET( c_server_httpbun + "delay/1" )
           .start( [ &cb_count ]( const auto & http ) { cb_count++; } );
      //
      uv_sleep( 200 );
      //
      http->abort();
      http->join();
      http->abort();
    }
    //
    auto start = uv_hrtime();
    async.stop(); // should wait for the end of all pending requests
    auto duration = uv_hrtime() - start;  // in nanoseconds.
    EXPECT_LT( duration, 3'000'000'000 ); // < 3s
  }
  //
  EXPECT_TRUE( cb_count == 1 );
  //
  {
    ASync async;
    async.start();
    //
    {
      auto http = HTTP::create( async );
      //
      http->GET( c_server_httpbun + "delay/1" )
           .start( [ &cb_count ]( const auto & http ) { cb_count++; } );
      //
      uv_sleep( 200 );
      //
      http->abort().join();
      //
      auto code = http->GET( c_server_httpbun + "get?a=19" ).exec().get_code();
      ASSERT_EQ( code, 200 );
      EXPECT_EQ( json_extract( http->get_body(), "$.args.a"), "19" );
    }
    //
    async.stop();
  }
}

//--------------------------------------------------------------------
// Request continues and ASync is stopped
TEST( http_complex, threaded_mode )
{
 {
    ASync async;
    async.start();
    //
    {
      auto http    = HTTP::create( async );
      long cb_code = 0;
      std::string cb_body;
      auto        code =
          http->GET( c_server_httpbun + "delay/1" )
              .threaded_callback( false )
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
      auto code = http->GET( c_server_httpbun + "get" )
                      .threaded_callback( false )
                      .exec()
                      .get_code();
      //
      EXPECT_EQ( code, 200 );
    }
    //
    async.stop();
  }
  //
  {
    bool done = false;
    //
    {
      ASync async;
      async.start();
      //
      {
        HTTP::create( async )
            ->GET( c_server_httpbun + "delay/1" )
            .threaded_callback( false )
            .start(
                [ &done ]( const auto & http )
                {
                  ASSERT_EQ( http.get_code(), 200 );
                  EXPECT_EQ( http.get_body(), "OK" );
                  done = true;
                } );
      }
      //
      async.stop(); // should wait for the end of all pending requests
    }
    //
    EXPECT_TRUE( done );
  }
}

//--------------------------------------------------------------------
// Validate the p_query_parameters handling in requests without body
TEST( http_complex, two_start )
{
  ASync async;
  async.start();
  //
  {
    bool cb1  = false;
    bool cb2  = false;
    auto http = HTTP::create( async );
    //
    http->GET( c_server_httpbun + "get" ).start( [ &cb1 ]( const auto & http ) { cb1 = true; } );
    http->GET( c_server_httpbun + "get" ).start( [ &cb2 ]( const auto & http ) { cb2 = true; } );
    //
    http->join();
    //
    EXPECT_TRUE( cb1 );
    EXPECT_FALSE( cb2 );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
TEST( http_complex, running )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    http->GET( c_server_httpbun + "delay/1" ) .start();
    //
    auto code = http->options( "connect_timeout=1,timeout=1" )
                     .authentication( "mode=basic,user=joe,secret=abc123" )
                     .certificates( "sslcert=unknown.pem,sslkey=unknown.pem,keypasswd=none" )
                     .get_code();
    EXPECT_EQ( code, c_running );
    //
    code = http->join().get_code();
    EXPECT_EQ( code, 200 );
  }
  //
  async.stop();
}
