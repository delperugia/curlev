#
# Top-level
#

cmake_minimum_required( VERSION 3.15 )
project( curlev VERSION 0.1 LANGUAGES CXX )

set( CMAKE_CXX_STANDARD          17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

include( CTest )
enable_testing()

# Mandatory packages
find_package     ( CURL      REQUIRED                       )
find_package     ( PkgConfig REQUIRED                       )
pkg_check_modules( LIBUV     REQUIRED IMPORTED_TARGET libuv )

# Library and test sources
add_subdirectory( src   )
if ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    add_subdirectory( tests )
endif()

#
# Extra targets
#

# Cppcheck
find_program( CPPCHECK_EXECUTABLE NAMES cppcheck )

if( CPPCHECK_EXECUTABLE )
    add_custom_target(
        cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE}
                --enable=all --inconclusive --force 
                --inline-suppr --std=c++17 --quiet
                --suppress=checkersReport
                --suppress=missingIncludeSystem
                --error-exitcode=1
                -I ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/src/ ${CMAKE_SOURCE_DIR}/tests/
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Cppcheck on all sources"
    )
endif()
