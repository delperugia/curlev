cmake_minimum_required(VERSION 3.15)
project(curlev VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
set(SRC_FILES
    src/async.cpp
    src/common.cpp
    src/http.cpp
    src/options.cpp
)

# Tests
set(TEST_FILES
    tests/test_main.cpp
    tests/test_1.cpp
    tests/test_2.cpp
)

# Add the library
add_library(curlev STATIC ${SRC_FILES})
target_include_directories(curlev PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(curlev PUBLIC uv curl)

# Test executable
add_executable(test_curlev ${TEST_FILES})
target_link_libraries(test_curlev PRIVATE curlev)

# cppcheck target
find_program(CPPCHECK_EXECUTABLE NAMES cppcheck)
if(CPPCHECK_EXECUTABLE)
    add_custom_target(
        cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE} --enable=all --inconclusive --inline-suppr --std=c++17 --force
                --quiet --suppress=missingIncludeSystem
                --suppress=unusedFunction --suppress=checkersReport 
                ${SRC_FILES} ${TEST_FILES}
                -I include
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck on sources"
    )
endif()

# valgrind target
find_program(VALGRIND_EXECUTABLE NAMES valgrind)
if(VALGRIND_EXECUTABLE)
    add_custom_target(
        valgrind
        COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --track-origins=yes ./test_curlev 99
        DEPENDS test_curlev
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running test_curlev with Valgrind"
    )
endif()

# Enable testing and add test
enable_testing()
add_test(NAME test_1 COMMAND test_curlev 1)
add_test(NAME test_2 COMMAND test_curlev 2)
