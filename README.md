curlev
======

[![CodeQL](https://github.com/delperugia/curlev/actions/workflows/github-code-scanning/codeql/badge.svg)](https://github.com/delperugia/curlev/actions/workflows/github-code-scanning/codeql)
[![flawfinder](https://github.com/delperugia/curlev/actions/workflows/flawfinder.yml/badge.svg)](https://github.com/delperugia/curlev/actions/workflows/flawfinder.yml)
[![Cppcheck](https://github.com/delperugia/curlev/actions/workflows/cppcheck.yml/badge.svg)](https://github.com/delperugia/curlev/actions/workflows/cppcheck.yml)
[![Clang-Tidy](https://github.com/delperugia/curlev/actions/workflows/clang-tidy.yml/badge.svg)](https://github.com/delperugia/curlev/actions/workflows/clang-tidy.yml)
[![Valgrind](https://github.com/delperugia/curlev/actions/workflows/valgrind.yml/badge.svg)](https://github.com/delperugia/curlev/actions/workflows/valgrind.yml)
[![Codecov](https://codecov.io/gh/delperugia/curlev/graph/badge.svg?token=339AIQXBS3)](https://codecov.io/gh/delperugia/curlev)
![MemorySanitizer](https://img.shields.io/badge/MemorySanitizer-enabled-blue)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

A C++ library providing a flexible and easy-to-use interface for making Internet requests.
It combines the power of `libcurl` for transfer operations with `libuv` for asynchronous I/O,
offering both synchronous and asynchronous request handling with minimal overhead.

Key features:

- Event driven (uses 4 times less CPU and 2 times less RAM than the best [libraries](docs/internals.md#performances))
- Synchronous and asynchronous requests with callback support
- Supports HTTP and SMTP
- Handles query parameters, form data, MIME handling, and raw bodies
- REST functions using common JSON parsers
- Custom headers and authentication
- Automatic retry on error
- Method chaining

Example of a synchronous request:

```cpp
#include <curlev/http.hpp>
#include <iostream>

using namespace curlev;

int main( int argc, char ** argv )
{
  ASync async;
  async.start();
  //
  auto http = HTTP::create( async );
  auto code = http->GET( "http://www.httpbin.org/get",
                          { { "name", "Alice" },
                            { "role", "admin" } } )
                  .exec()
                  .get_code();
  //
  std::cout << code << " " << http->get_body() << std::endl;
}
```

An asynchronous request later joined:

```cpp
#include <curlev/http.hpp>
#include <iostream>

using namespace curlev;

int main( int argc, char ** argv )
{
  ASync async;
  async.start();
  //
  auto http = HTTP::create( async );
  http->GET( "http://www.httpbin.org/get", { { "id", "42" } } )
      .start();
  //
  int value;
  std::cout << "Enter a number:" << std::endl;
  std::cin >> value;
  //
  auto code = http->join().get_code();
  std::cout << code << " " << http->get_body() << std::endl;
}
```

The same using a `std::future`:

```cpp
#include <curlev/http.hpp>
#include <iostream>

using namespace curlev;

int main( int argc, char ** argv )
{
  ASync async;
  async.start();
  //
  auto future = HTTP::create( async )->GET( "http://www.httpbin.org/get", { { "id", "42" } } )
                                      .launch();
  //
  int value;
  std::cout << "Enter a number:" << std::endl;
  std::cin >> value;
  //
  auto response = future.get();
  std::cout << response.code << " " << response.body << std::endl;
}
```

A request with callback, running detached:

```cpp
#include <chrono>
#include <curlev/http.hpp>
#include <iostream>
#include <thread>

using namespace curlev;

void f( ASync & async )
{
  auto http = HTTP::create( async );
  http->GET( "http://www.httpbin.org/get" )
      .start( []( const HTTP & http )
              { std::cout << http.get_code() << " " << http.get_body() << std::endl; } );
}

int main( int argc, char ** argv )
{
  ASync async;
  async.start();
  //
  f( async );
  //
  std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
}
```

The complete reference manual can be found [here](docs/reference_manual.md).

# Installing and compiling

In order to compile `curlev` you will need the following:

 - git
 - cmake
 - pkg-config
 - a C++17 compiler
 - libcurl (>=7.61.1 but with memory leaks and config problems, >=7.87.0 recommended)
 - libuv
 - optionally: RapidJSON or nlohmann/json

Here are the package names for some distributions:

Distribution | packages
-------------|-----------------------
Ubuntu       | git, cmake, pkg-config, g++ or clang, libcurl4-openssl-dev or libcurl4-gnutls-dev, libuv1-dev
Suse         | git, cmake, pkg-config, gcc-c++ or clang, libcurl-devel, libuv-devel
Oracle       | git, cmake, pkgconf-pkg-config, g++, libcurl-devel, libuv-devel

In a terminal, execute the following commands:

```sh
git clone https://github.com/delperugia/curlev
cd curlev/
cmake      -B         build/  -DCMAKE_BUILD_TYPE=Release
cmake      --build    build/  -j
sudo cmake --install  build/
```

If `RapidJSON` or `nlohmann/json` are installed, extra `REST` methods
will be available in the HTTP client.

# Testing and debugging

For testing and debugging, you must also install the following packages:

 - clang-tidy
 - cppcheck (>=2.13)
 - gtest
 - nlohmann-json
 - valgrind

In a console, execute the following:

```sh
git clone https://github.com/delperugia/curlev
cd curlev/
cmake -B         build/  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTING=ON
cmake --build    build/  -j
ctest --test-dir build/  -T test -T coverage
cmake --build    build/  --target cppcheck
cmake --build    build/  --target clang-tidy
cmake --build    build/  --target clean
```

Tested with:
|         | Suse 15 | Oracle 8.6 | Ubuntu 22.04| Ubuntu 24.04|
|---------|---------|------------|-------------|-------------|
| g++     |         | 8.5.0      |             | 13.3.0      |
| clang   | 7.0.1   |            | 14.0.0      |             |
| libuv   | 1.44.2  | 1.41.1     | 1.43.0      | 1.48.0      |
| libcurl | 8.6.0   | 7.61.1     | 7.81.0      | 8.5.0       |
