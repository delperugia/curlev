
# Tests
set(TEST_1_FILES
    test_utils.cpp
    test_1_common.cpp
    test_1_http_basic.cpp
    test_1_http_advanced.cpp
    test_1_http_complex.cpp
)

# valgrind target
find_program(VALGRIND_EXECUTABLE NAMES valgrind)
if(VALGRIND_EXECUTABLE)
    add_custom_target(
        valgrind
        COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --track-origins=yes ./test_1 99
        DEPENDS test_1
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running test_curlev with Valgrind"
    )
endif()

# Enable testing and add test
enable_testing()
find_package(GTest REQUIRED)
find_package(nlohmann_json REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
include(CTest)

add_executable(test_1 ${TEST_1_FILES})
target_link_libraries(test_1 GTest::gtest GTest::gtest_main curlev nlohmann_json::nlohmann_json)

add_test(NAME Test_1 COMMAND test_1)
