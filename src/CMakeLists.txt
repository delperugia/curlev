# Sources
set( SRC_FILES
     async.cpp
     authentication.cpp
     common.cpp
     http.cpp
     options.cpp
)

# Add the library
find_package( CURL REQUIRED )

add_library               ( curlev STATIC ${SRC_FILES} )
target_include_directories( curlev PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include )
target_link_libraries     ( curlev PRIVATE CURL::libcurl )

find_package( PkgConfig )
if ( PkgConfig_FOUND )
    pkg_check_modules    ( UV_LIBRARY REQUIRED IMPORTED_TARGET libuv )
    target_link_libraries( curlev PRIVATE PkgConfig::UV_LIBRARY )
else()
    find_library( UV_LIBRARY NAMES uv REQUIRED )
    if ( UV_LIBRARY )
        target_link_libraries(curlev PRIVATE ${UV_LIBRARY})
    else()
        message(FATAL_ERROR "libuv not found")
    endif()
endif()

# Compilation
target_compile_options( curlev PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall -Wextra -Wpedantic -Wcast-align -Wcast-qual -Wformat=2
        -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow
        -Wswitch -Wswitch-default -Wundef
    >
    $<$<CXX_COMPILER_ID:GNU>: -Wlogical-op >
)
if ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    add_compile_options( -g )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release" )
    add_compile_options( -O2 )
endif()

# Installation
include( GNUInstallDirs )
install( TARGETS   curlev                                   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/  DESTINATION         ${CMAKE_INSTALL_INCLUDEDIR}/curlev )
