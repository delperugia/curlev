#include "async.hpp"
#include "http.hpp"

static ASync g_async;

bool test_1_a( void )
{
    long code;
    auto http = HTTP::create( g_async );
    //
    code = http->options( "verbose=0" )
               .add_header( "AA", "aa" )
               .add_headers( { { "BB", "bb" }, { "CC", "cc" } } )
               .do_post( "http://httpbin.org/post", { { "x", "42" }, { "y", "43" } } )
               .exec()
               .get_code();
    //
    printf( " code = %ld count = %zu body = %s\n", code, http.use_count(), http->get_body().c_str() );
    //
    return code == 200 && http.use_count() == 1;
}

bool test_1( void )
{
    bool ok = g_async.start() && test_1_a();
    //
    g_async.stop();
    //
    return ok;
}