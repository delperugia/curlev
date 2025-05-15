#include "async.hpp"
#include "http.hpp"

bool test_1( void );
bool test_2( void );

int main( int argc, const char * argv[] )
{
    if ( argc != 2 )
    {
        puts( "Missing test number" );
        exit( 1 );
        return 1;
    }
    //
    bool ok = false;
    //
    switch ( atoi( argv[ 1 ] ) )
    {
        case 1 : ok = test_1(); break;
        case 2 : ok = test_2(); break;
        case 99: ok = test_1() && test_2(); break;
        default:
            puts( "Invalid test number" );
            exit( 1 );
            return 1;
    }
    //
    if ( ! ok )
    {
        puts( "Test failed" );
        exit( 1 );
        return 1;
    }
    //
    return 0;
}
