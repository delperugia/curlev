#include <gtest/gtest.h>
#include <iostream>

#include "async.hpp"
#include "common.hpp"
#include "http.hpp"

TEST( method, equivalence )
{
    ASync async;
    async.start();
    //
    {
        auto http = HTTP::create( async );
        auto code = http->do_get( "http://www.httpbin.org/get" ).exec().get_code();
        std::cout << code << " " << http->get_body() << std::endl;
        //
        EXPECT_EQ( code, 200 );
    }
    {
        auto http = HTTP::create( async );
        auto code = http->do_post( "http://www.httpbin.org/post",
                                   { { "name", "Alice" }, { "role", "admin" } } )
                        .exec()
                        .get_code();
        std::cout << code << " " << http->get_body() << std::endl;
        //
        EXPECT_EQ( code, 200 );
    }
    //
    async.stop();
}
