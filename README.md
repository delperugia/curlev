curlev
======

[![CodeQL](https://github.com/delperugia/curlev/actions/workflows/github-code-scanning/codeql/badge.svg)](https://github.com/delperugia/curlev/actions/workflows/github-code-scanning/codeql)
[![flawfinder](https://github.com/delperugia/curlev/actions/workflows/flawfinder.yml/badge.svg)](https://github.com/delperugia/curlev/actions/workflows/flawfinder.yml)
[![Cppcheck](https://github.com/delperugia/curlev/actions/workflows/cppcheck.yml/badge.svg)](https://github.com/delperugia/curlev/actions/workflows/cppcheck.yml)
[![Valgrind](https://github.com/delperugia/curlev/actions/workflows/valgrind.yml/badge.svg)](https://github.com/delperugia/curlev/actions/workflows/valgrind.yml)
[![Codecov](https://codecov.io/gh/delperugia/curlev/graph/badge.svg?token=339AIQXBS3)](https://codecov.io/gh/delperugia/curlev)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

A C++ HTTP library providing a flexible and easy-to-use interface for making HTTP requests.
It combines the power of `libcurl` for HTTP operations with `libuv` for asynchronous I/O,
offering both synchronous and asynchronous request handling using a limited overhead.

Key features:

- Event driven (4 times less CPU and 2 times less RAM used in some cases)
- Synchronous and asynchronous requests with callback support
- All standard HTTP methods (GET, POST, PUT, PATCH, DELETE)
- Query parameters, form data and MIME handling
- Custom headers and authentication
- Method chaining

Example of a simple synchronous request:

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

And a simple asynchronous request with callback:

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
  http->GET( "http://www.httpbin.org/get",
             { { "name", "Alice" },
               { "role", "admin" } } )
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

The complete reference manual can be found [here](docs/reference_manual.md).

Its architecture and code are ready to give access to the other protocols
provided by `libcurl`, but only HTTP is available today.

# Installing and compiling

In order to compile `curlev` you will need the following:

 - git
 - cmake
 - pkg-config
 - a C++17 compiler
 - libcurl (>=7.61.1, >=7.87.0 recommended)
 - libuv

Here are the package name for some distribution:

Distribution | packages
-------------|-----------------------
Ubuntu       | git, cmake, pkg-config, g++ or clang, libcurl4-openssl-dev or libcurl4-gnutls-dev, libuv1-dev
Suse         | git, cmake, pkg-config, gcc-c++ or clang, libcurl-devel, libuv-devel
Oracle       | git, cmake, pkgconf-pkg-config, g++, libcurl-devel, libuv-devel

In a console, execute the following:

```sh
git clone https://github.com/delperugia/curlev
cd curlev/
cmake      -B         build/  -DCMAKE_BUILD_TYPE=Release
cmake      --build    build/  -j
sudo cmake --install  build/
```

# Testing and debugging

For testing and debugging, the following extra packages must be installed:

 - gtest
 - nlohmann-json
 - cppcheck (>=2.13)
 - valgrind

In a console, execute the following:

```sh
git clone https://github.com/delperugia/curlev
cd curlev/
cmake -B         build/  -DCMAKE_BUILD_TYPE=Debug
cmake --build    build/  -j
ctest --test-dir build/  -T test -T coverage
ctest --test-dir build/  -T memcheck
cmake --build    build/  --target cppcheck
cmake --build    build/  --target clean
```

Tested with:
 - g++        8.5.0   11.4.0          13.3.0 
 - clang                      14.0.0
 - libuv      1.41.1  1.43.0  1.43.0  1.48.0
 - libcurl    7.61.1  7.81.0  7.81.0  8.5.0 
