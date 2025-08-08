#
# Sources
#

# Files
set( SRC_FILES
     async.cpp
     authentication.cpp
     certificates.cpp
     http.cpp
     http_json.cpp
     mime.cpp
     options.cpp
     utils/curl_utils.cpp
     utils/map_utils.cpp
     utils/string_utils.cpp
)

# Check strcasecmp availability
include( CheckFunctionExists )
check_function_exists( strcasecmp HAVE_STRCASECMP )
check_function_exists( _stricmp   HAVE__STRICMP   )
if( NOT HAVE_STRCASECMP AND NOT HAVE__STRICMP )
    add_definitions( -DNEED_PORTABLE_STRCASECMP )
elseif( HAVE__STRICMP )
    add_definitions( -Dstrcasecmp=_stricmp )
endif()

# The curlev library
add_library               ( curlev STATIC ${SRC_FILES}                           )
target_include_directories( curlev PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include )
target_link_libraries     ( curlev PUBLIC CURL::libcurl
                                          PkgConfig::LIBUV )

# Compilation options
target_compile_options( curlev PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall -Wextra -Wpedantic -Wcast-align -Wcast-qual -Wformat=2
        -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow
        -Wswitch -Wswitch-default -Wundef -Werror
    >
)

if ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    target_compile_options( curlev PRIVATE -O0 --coverage )
endif()

# Installation
include( GNUInstallDirs )
install( TARGETS   curlev                                   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/  DESTINATION         ${CMAKE_INSTALL_INCLUDEDIR}/curlev )
