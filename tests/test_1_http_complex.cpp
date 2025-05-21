#include <gtest/gtest.h>
#include <iostream>

#include "async.hpp"
#include "common.hpp"
#include "http.hpp"
#include "test_utils.hpp"

TEST( http, simultaneous )
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
      https.back()->GET( c_server + "status/" + expected_code ).start( nullptr );
    }
    //
    for ( auto i = 0; i < codes.size(); i++ )
      EXPECT_EQ( std::to_string( https[ i ]->join().get_code() ), codes[ i ] );
  }
  //
  async.stop();
}

TEST( http, headers )
{
  // send specific header
}

TEST( http, post_json )
{
  // with json body
  // with query & json body
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
