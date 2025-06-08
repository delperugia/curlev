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
      auto code = http->GET( c_server + "get" ).exec().get_code();
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
TEST( http_basic, method_equivalence )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code = http->DELETE( c_server + "delete" ).exec().get_code();
    //
    EXPECT_EQ( code, 200 );
  }
  {
    auto http = HTTP::create( async );
    auto code = http->GET( c_server + "get" ).exec().get_code();
    //
    EXPECT_EQ( code, 200 );
  }
  {
    auto http = HTTP::create( async );
    auto code = http->PATCH( c_server + "patch" ).exec().get_code();
    //
    EXPECT_EQ( code, 200 );
  }
  {
    auto http = HTTP::create( async );
    auto code = http->POST( c_server + "post" ).exec().get_code();
    //
    EXPECT_EQ( code, 200 );
  }
  {
    auto http = HTTP::create( async );
    auto code = http->PUT( c_server + "put" ).exec().get_code();
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
    auto code = http->GET( c_server + "get" ).exec().get_code();
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
    auto code = http->GET( c_server + "get", { { "a", "11" } } ).exec().get_code();
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
    auto code = http->GET( c_server + "get", { { "bb", "23" }, { "a", "21" } } ).exec().get_code();
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
    auto code = http->GET( c_server + "get?ax=31", { { "bx", "32" } } ).exec().get_code();
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
    auto code = http->POST( c_server + "post" ).exec().get_code();
    ASSERT_EQ( code, 200 );
    //
    EXPECT_EQ( json_count( http->get_body(), "$.args"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.form"  ), 0 );
    EXPECT_EQ( json_count( http->get_body(), "$.files" ), 0 );
  }
  //
  {
    auto http = HTTP::create( async );
    auto code = http->POST( c_server + "post", { { "a", "1" }, { "b", "2" } } ).exec().get_code();
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
    auto code = http->POST( c_server + "post", {} )
                    .add_query_parameters( { { "a", "1" }, { "b", "2" } } )
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
    auto code = http->POST( c_server + "post", { { "a", "1" } } )
                    .add_query_parameters( { { "b", "2" } } )
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
    auto code = http->POST( c_server + "post", { { "b1", "1" } } )
                    .add_query_parameters(     { { "q1", "2" } } )
                    .add_body_parameters (     { { "b2", "3" } } )
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
TEST( http_basic, post_mime )
{
  ASync async;
  async.start();
  //
  {
    auto http = HTTP::create( async );
    auto code =
        http->POST( c_server + "post" )
            .add_mime_parameters( { HTTP::t_mime_parameter{ "m1", "40" } } )
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
        http->POST( c_server + "post" )
            .add_mime_parameters( { ( HTTP::t_mime_parameter{ "m2", "42" } ),
                                    ( HTTP::t_mime_parameter{ "m3", "43" } ) } )
            .add_query_parameters( { { "q4", "44" } } )
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
            .add_mime_parameters( { HTTP::t_mime_file{ "f1", "text/plain", "Hello!", "f1.txt" } } )
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
        http->POST( c_server_httpbun + "post" )
            .add_mime_parameters( { ( HTTP::t_mime_parameter{ "m21", "51" } ),
                                    ( HTTP::t_mime_parameter{ "m22", "52" } ),
                                    ( HTTP::t_mime_file     { "f21", "text/html", "World", "f21.txt" } ) } )
            .add_query_parameters( { { "q23", "33" } } )
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
    EXPECT_EQ( json_extract( http->get_body(), "$.files.f21.content"              ), "World" );
    EXPECT_EQ( json_extract( http->get_body(), "$.files.f21.filename"             ), "f21.txt" );
    EXPECT_EQ( json_extract( http->get_body(), "$.files.f21.headers.Content-Type" ), "text/html" );
  }
  //
  async.stop();
}
