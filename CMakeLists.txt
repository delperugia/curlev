cmake_minimum_required( VERSION 3.15 )
project( curlev VERSION 0.1 LANGUAGES CXX )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

add_subdirectory( src )
add_subdirectory( tests )

# Cppcheck target
find_program( CPPCHECK_EXECUTABLE NAMES cppcheck )
if( CPPCHECK_EXECUTABLE )
    add_custom_target(
        cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE} --enable=all --inconclusive --inline-suppr --std=c++17 --force
                --quiet --suppress=missingIncludeSystem
                --suppress=unusedFunction --suppress=checkersReport 
                -I ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/src/ ${CMAKE_SOURCE_DIR}/tests/
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Cppcheck on sources"
    )
endif()
