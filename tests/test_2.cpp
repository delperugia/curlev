#include "async.hpp"
#include "http.hpp"

bool test_2_a( void )
{
    return true;
}

bool test_2( void )
{
    return test_2_a();
}
