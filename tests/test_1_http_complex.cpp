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
      https.back()->GET( c_server + "status/" + expected_code ).start();
    }
    //
    for ( auto i = 0; i < codes.size(); i++ )
      EXPECT_EQ( std::to_string( https[ i ]->join().get_code() ), codes[ i ] );
    //
    EXPECT_EQ( async.get_running_request_max(), codes.size() );
    EXPECT_EQ( async.get_running_request() , 0            );
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
          ->GET( c_server + "delay/1" )
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
    auto code = http1->GET( c_server + "cookies/set", { { "d1", "61" } } )
      .options ( "cookies=1" )
      .exec().get_code();
    ASSERT_EQ( code, 302 );
    //
    code = http1->GET( c_server + "cookies" )
      .options ( "cookies=1" )
      .exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_extract( http1->get_body(), "$.cookies.d1" ), "61" );
    //
    code = http2->GET( c_server + "cookies" )
      .exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_extract( http2->get_body(), "$.cookies.d1" ), "" );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server + "cookies" )
      .exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_extract( http->get_body(), "$.cookies.d1" ), "" );
  }
  //
  {
    auto http1 = HTTP::create( async );
    auto http2 = HTTP::create( async );
    //
    auto code = http1->GET( c_server + "cookies/set", { { "e1", "71" } } )
      .options ( "cookies=1" )
      .exec().get_code();
    ASSERT_EQ( code, 302 );
    //
    code = http2->GET( c_server + "cookies/set", { { "e2", "72" } } )
      .options ( "cookies=1" )
      .exec().get_code();
    ASSERT_EQ( code, 302 );
    //
    code = http2->GET( c_server + "cookies" )
      .options ( "cookies=1" )
      .exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_extract( http2->get_body(), "$.cookies.e2" ), "72" );
    //
    code = http1->GET( c_server + "cookies" )
      .options ( "cookies=1" )
      .exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_extract( http1->get_body(), "$.cookies.e1" ), "71" );
  }
  //
  async.stop();
}

//--------------------------------------------------------------------
// Validate the p_query_parameters handling in requests without body
TEST( http_basic, redirect )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server + "redirect", { { "url", "http://somewhere.com/" } } )
                     .exec().get_code();
    ASSERT_EQ( code, 302 );
    //
    EXPECT_EQ(  http->get_redirect_url(), "http://somewhere.com/" );
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
      http->GET( c_server + "delay/1" )
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
      http->GET( c_server + "delay/1" )
           .start( [ &cb_count ]( const auto & http ) { cb_count++; } );
      //
      uv_sleep( 200 );
      //
      http->abort().join();
      //
      auto code = http->GET( c_server + "get?a=19" ).exec().get_code();
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
          http->GET( c_server + "delay/1" )
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
      auto code = http->GET( c_server + "get" )
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
            ->GET( c_server + "delay/1" )
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