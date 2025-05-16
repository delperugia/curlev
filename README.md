curlev
======

Why not CPR: pool of threads, this uses only one worker thread.

Returns a shared pointer because it is asynchronous, and the invoker can continue without
waiting for the completion.

# Installation

sudo apt-get install g++ libcurl4-openssl-dev libuv1-dev cmake pkg-config make

libcurl: minimal version is v7.69. v8.5.0 that come with Ubuntu 24.04 is buggy.

# Build & tests

cmake CMakeLists.txt -B build
cmake --build build
ctest --test-dir build

cmake --build build --target cppcheck
cmake --build build --target valgrind
