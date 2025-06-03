#
# Tests
#

# Files
set( TEST_1_FILES
     test_utils.cpp
     test_1_common.cpp
     test_1_http_basic.cpp
     test_1_http_advanced.cpp
     test_1_http_complex.cpp
)

# Tests are optionals, depending on the presence of some packages
find_package( GTest         )
find_package( nlohmann_json )

if ( GTest_FOUND AND nlohmann_json_FOUND )
    include_directories( ${GTEST_INCLUDE_DIRS} )
    #
    set( MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1" )
    #
    add_executable( test_1 ${TEST_1_FILES} )
    target_compile_options( test_1 PRIVATE -g -coverage )
    target_link_options   ( test_1 PRIVATE -coverage )
    target_link_libraries ( test_1
                            curlev 
                            GTest::gtest
                            GTest::gtest_main 
                            nlohmann_json::nlohmann_json )
    #
    add_test( NAME Test_1 COMMAND test_1 )
endif()
