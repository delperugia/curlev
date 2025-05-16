curlev
======

This library is based on the libcurl example [multi-uv](https://curl.se/libcurl/c/multi-uv.html]).

Returns a shared pointer because it is asynchronous, and the invoker can continue without
waiting for the completion.

# Examples

```cpp
ASync async;
async.start();
```

```cpp
auto http = HTTP::create( async );
auto code = http->do_get( "http://www.httpbin.org/get" ).exec().get_code();
std::cout << code << " " << http->get_body() << std::endl;
```

```cpp
auto http = HTTP::create( async );
auto code = http->do_post( "http://www.httpbin.org/post",
                            { { "name", "Alice" },
                              { "role", "admin" } } )
                 .exec()
                 .get_code();
std::cout << code << " " << http->get_body() << std::endl;
```

example: async

example: callback

example: detached

# Installation

```sh
sudo apt-get install g++ libcurl4-openssl-dev libuv1-dev cmake pkg-config make libgtest-dev
```

 - libcurl: minimal version is v7.69. v8.5.0 that come with Ubuntu 24.04 is buggy.

# Build & tests

```sh
cmake CMakeLists.txt -B build
cmake --build build
ctest --test-dir build
cmake --build build --target cppcheck
cmake --build build --target valgrind
```

# Q & A

## Why not using CPR
CPR uses a pool of threads, when running numerous simultaneous requests,
this becomes a real problem. In CPR, asynchronous is an separate interface.

## Why is there no JSON interface?
There are numerous JSON library (RapidJSON, nlohmann, simdjson...) and the one
I would have used probably wouldn't have been your preferred choice or the one
you used in your project.

# References

 - [libcurl](https://curl.se/libcurl/)
 - [libuv](https://libuv.org/)
 - libcurl example [multi-uv](https://curl.se/libcurl/c/multi-uv.html])
 - [GoogleTest](https://google.github.io/googletest/)
