#********************************************************************
# Copyright (c) 2025, Pierre DEL PERUGIA and the curlev project contributors
# SPDX-License-Identifier: Apache-2.0
#*******************************************************************

# Top-level

cmake_minimum_required( VERSION 3.15 )
project(
    curlev
    VERSION     0.2.0
    DESCRIPTION "An event-based C++ wrapper for libcurl"
    LANGUAGES   CXX
)

set( CMAKE_CXX_STANDARD          17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

# Mandatory packages
find_package     ( CURL      REQUIRED                       )
find_package     ( PkgConfig REQUIRED                       )
pkg_check_modules( LIBUV     REQUIRED IMPORTED_TARGET libuv )

# Adding Sanitizers
if ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    add_compile_options(-fsanitize=undefined -fsanitize=address -fsanitize=leak -fno-sanitize-recover=all)
    add_link_options   (-fsanitize=undefined -fsanitize=address -fsanitize=leak)
endif()

# Generate version.hpp from template
configure_file(
    ${PROJECT_SOURCE_DIR}/version.hpp.in
    ${CMAKE_BINARY_DIR}/include/version.hpp
    @ONLY
)

# Library and test sources
add_subdirectory( src )

option( BUILD_TESTING "Build the test executables" OFF )
if ( BUILD_TESTING )
    include( CTest )
    enable_testing()
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
                -I ${CMAKE_SOURCE_DIR}/include -I ${CMAKE_BINARY_DIR}/include
                ${CMAKE_SOURCE_DIR}/src/ ${CMAKE_SOURCE_DIR}/tests/
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Cppcheck on all sources"
    )
endif()

# clang-tidy
find_program( CLANG_TIDY_EXECUTABLE NAMES clang-tidy )

if( CLANG_TIDY_EXECUTABLE )
    add_custom_target(
        clang-tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE}
                -p ${CMAKE_SOURCE_DIR}/build/
                ${CMAKE_SOURCE_DIR}/include/*.hpp ${CMAKE_SOURCE_DIR}/src/*.cpp
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on all sources"
    )
endif()
