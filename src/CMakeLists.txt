# Sources
set(SRC_FILES
    async.cpp
    authentication.cpp
    common.cpp
    http.cpp
    options.cpp
)

# Add the library
add_library(curlev STATIC ${SRC_FILES})
target_include_directories(curlev PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(curlev PUBLIC uv curl)
