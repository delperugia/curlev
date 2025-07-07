curlev - reference manual
=========================

All classes and functions are inside the namespace `curlev`.
The only header to include is `curlev/http.hpp`.

# Quickstart guide

 - Include <curlev/http.hpp>
 - Instantiate a global `ASync` object
 - To do a request:
   - create an `HTTP` object using `HTTP::create()`
   - call one of `GET()`, `DELETE()`, `POST()`, `PUT()` or `PATCH()` (or `REST()`)
   - optionally call `add_headers()`, `add_..._parameters()`, `options()`, `headers()`...
   - call `exec()` (synchronous), `start()`/`join()` (asynchronous), or `launch()` (future)
   - call the `get_...()` methods
- Notes:
  - if used, the callback in `start()` must be as short as possible
  - `ASync`'s `stop()` must be called after all `HTTP` objects are released

# Starting

The `libcurl` and `libuv` libraries must be started before any other operations,
usually when the application starts. Usually a single instance of `ASync` is needed,
but several can be created if needed. An instance is started using:

```cpp
m_async.start();
```

Similarly it is stopped when the application terminates using:

```cpp
m_async.stop();
```

`stop()` waits for all pending requests to finish before returning.
If a timeout occurs (30s by default), it returns `true`.
If there is still existing protocol objects (like HTTP) when
this function is called, memory leaks will occur:

```cpp
ASync async;
async.start();
auto http = HTTP::create( async );
http->GET( c_server + "get" ).exec().get_code();
async.stop();
```

causes a leak because `http` (which holds an easy curl handle) still exist
when `ASync` (and libcurl) is stopped (technically `curl_share_cleanup` is called
while a `CURL` handle still has a reference on the `CURLSH` handle).

## Default configuration

The default configuration of the `HTTP` instances can be set in `ASync` using
the two methods `options()`, `authentication()` and `certificates()`.
The parameters are the same as the ones described in `HTTP`.

# Creating an HTTP instance

The static method `create()` returns a `shared_ptr` on the new object. Using a `shared_ptr`
and its reference counter allows to have an object that can continues to run detached from
the creation context.

```cpp
auto http = HTTP::create( async );  // async is the instance of ASync
```

# Building the request

On the `HTTP` object, you first have to call one of the HTTP methods.
This will restart a new request sessions.
Available methods are `GET()`, `DELETE()`, `POST()`, `PUT()` and `PATCH()`.

`GET()` and `DELETE()` are requests without body and expect an URL and
an optional map of parameters to send on the query string.

`DELETE()`, `POST()`, `PUT()` and `PATCH()` have three forms:
1. URL
2. URL, content type and body
3. URL and a map of parameters to send in the body as `application/x-www-form-urlencoded`

Then if needed, one or several of the following configuration methods
are available:
- `add_query_parameters( params )`: add query parameters
- `add_body_parameters( params )`:  add body parameters (forms 1 and 3 only)
- `add_mime_parameters( parts )`:   add MIME parts to the body (form 1 only)
- `add_headers( headers )`:         add headers
- `authentication( auth_string )`:  set authentication
- `options( opt_string )`:          set options
- `certificates( cert_string )`:    set SSL certificates

## REST

If `curlev` was compiled with RapidJSON or nlohmann/json, the `HTTP` object
has an extra method `REST()`.

`REST()` has two forms:
1. URI, verb (any of 'GET', 'DELETE', 'PATCH', 'POST' or 'PUT')
2. URI, verb (only 'PATCH', 'POST' or 'PUT') and a JSON object (a `rapidjson::Document` or a `nlohmann::json`)

## Adding headers and parameters

`add_headers()`, `add_query_parameters()` and `add_body_parameters()`
expect an unordered map of keys/parameters and values: `std::unordered_map< std::string, std::string >`:

```cpp
add_headers( { { "Accept"         , "text/html" },
               { "Accept-Language", "en-US"     } } ).
add_query_parameters( { { "p1", "1" },
                        { "p2", "2" } } )
```

`add_query_parameters()` will add them as query parameters in the URL,
`add_body_parameters()` in the request body as `application/x-www-form-urlencoded`.

## Adding MIME parts

The `add_mime_parameters` method expects a vector of MIME parts:
- `mime::parameter` to add a simple name/value parameter
  - fields are: `name`, `value`
```cpp
add_mime_parameters( { mime::parameter{ "p1", "1" },
                       mime::parameter{ "p2", "2" } } )
```
- `mime::data`      to add data part, with an optional Content-Type and remote file name
  - fields are: `name`, `data`, `content_type`, `filename`
```cpp
add_mime_parameters( { mime::parameter{ "p1", "1" },
                       mime::data     { "p2", "Hello, world!", "text/plain", "f.txt" } } )
```
- `mime::file`      to read data from a file as a part, with an optional Content-Type and remote file name
  - fields are: `name`, `filedata`, `content_type`, `filename`
  - default `filename` is the base name of `filedata`
```cpp
add_mime_parameters( { mime::parameter{ "p1", "1" },
                       mime::file     { "p2", "/tmp/uZ7hHC2", "text/plain", "f.txt" } } )
```

## Adding authentication

The string expected by the `authentication()` is a key-value comma
separated string with the following keys available:

| Name   | Comment                                         | libcurl option
|--------|-------------------------------------------------|--------------------
| mode   | "none", "basic", "digest" or "bearer"           | CURLAUTH_NONE, CURLAUTH_BASIC, CURLAUTH_DIGEST or CURLAUTH_BEARER
| user   | for "basic" and "digest" modes only: user login | CURLOPT_USERNAME
| secret | password or token                               | CURLOPT_PASSWORD or CURLOPT_XOAUTH2_BEARER

For example:
- mode=basic,user=joe,secret=abc123
- mode=bearer,secret=ABCDEFHIJKLMNOQRSTUVWXYZ

## Setting options

The string expected by the `options()` is a key-value comma
separated string with the following keys available:

| Name               | Default | Unit         | Comment                             | libcurl option
|--------------------|---------|--------------|-------------------------------------|---------------------
| accept_compression | 0       | 0 or 1       | activate compression                | CURLOPT_ACCEPT_ENCODING
| connect_timeout    | 30000   | milliseconds | connection timeout                  | CURLOPT_CONNECTTIMEOUT_MS
| cookies            | false   | 0 or 1       | receive and resend cookies          | CURLOPT_COOKIEFILE
| follow_location    | 0       | 0 or 1       | follow HTTP 3xx redirects           | CURLOPT_FOLLOWLOCATION
| insecure           | 0       | 0 or 1       | disables certificate validation     | CURLOPT_SSL_VERIFYHOST and CURLOPT_SSL_VERIFYPEER
| maxredirs          | 5       | count        | maximum number of redirects allowed | CURLOPT_MAXREDIRS
| proxy              |         | string       | the SOCKS or HTTP URl to a proxy    | CURLOPT_PROXY
| timeout            | 30000   | milliseconds | receive data timeout                | CURLOPT_TIMEOUT_MS
| verbose            | 0       | 0 or 1       | debug log on console                | CURLOPT_VERBOSE

For example:
- follow_location=1,timeout=5000
- insecure=1
- proxy=socks5://user123:pass456@192.168.1.100:1080

Notes:
- accept_compression: all built-in supported encodings are accepted
- cookies:            no initial file is specified when activated
- proxy:              see https://curl.se/libcurl/c/CURLOPT_PROXY.html

## Setting certificates

The string expected by the `certificates()` is a key-value comma
separated string with the following keys available:

| Connection | Usage    | Key               | Comment                                        | libcurl option
|------------|----------|-------------------|------------------------------------------------|---------------------------
| -          | global   | engine            | engine or provider name                        | CURLOPT_SSLENGINE
| direct     | public   | sslcert           | file                                           | CURLOPT_SSLCERT
| "          | "        | sslcerttype       | "PEM", "DER" or "P12", default "PEM"           | CURLOPT_SSLCERTTYPE
| "          | private  | sslkey            | file or id                                     | CURLOPT_SSLKEY
| "          | "        | sslkeytype        | "PEM", "DER", "ENG" or "PROV", default "PEM"   | CURLOPT_SSLKEYTYPE
| "          | "        | keypasswd         | -                                              | CURLOPT_KEYPASSWD
| "          | CA       | cainfo            | file                                           | CURLOPT_CAINFO
| "          | "        | capath            | folder                                         | CURLOPT_CAPATH
| proxy      | public   | proxy_sslcert     | file                                           | CURLOPT_PROXY_SSLCERT
| "          | "        | proxy_sslcerttype | "PEM", "DER" or "P12", default "PEM"           | CURLOPT_PROXY_SSLCERTTYPE
| "          | private  | proxy_sslkey      | file or id                                     | CURLOPT_PROXY_SSLKEY
| "          | "        | proxy_sslkeytype  | "PEM", "DER", "ENG" or "PROV", default "PEM"   | CURLOPT_PROXY_SSLKEYTYPE
| "          | "        | proxy_keypasswd   | -                                              | CURLOPT_PROXY_KEYPASSWD
| "          | CA       | proxy_cainfo      | file                                           | CURLOPT_PROXY_CAINFO
| "          | "        | proxy_capath      | folder                                         | CURLOPT_PROXY_CAPATH

For example:
 - sslcert=client.pem,sslkey=key.pem,keypasswd=s3cret

Note: before `libcurl` 7.84.0 it is not possible to reset Certificate Authorities to their default
values. If one of the keys `cainfo`, `capath`, `proxy_cainfo` or `proxy_capath` is changed in a request,
it must be changed for all the requests made using the same HTTP object.

# Executing the request

Once the request is ready, it can be started using `start()`.
The request then runs asynchronously. This method accepts a callback function
that will be called once the request is finished, successfully or not.
Then call `join()` to wait for asynchronous request completion.

Or the request can be started using the convenient method `exec()`,
which executes the request synchronously (`start()` + `join()`).

Or a `std::future` can be retrieved using `launch()`.
Note that because the response is copied to the future,
it is less efficient than using `start()` + `join()` or `exec()`

While a request is running, all methods except `join()` are ignored
(as a side effect `exec()` behaves like `join()`).

## Callback

If a callback is passed to `start()`, it is invoked before
the `join()` is released.

By default the callback is invoked from a separate thread. This prevents
the main IO thread from being blocked while processing the callback, but
adds an extra overhead. As with all callbacks, it must return quickly.

If the callback is known to be very fast, it is possible to invoke it
directly from the IO thread by changing the default mode
using `threaded_callback()` method of the HTTP instance:

```cpp
http->GET( "http://www.httpbin.org/get",
           { { "name", "Alice" }, { "role", "admin" } } )
     .threaded_callback( false )
     .start( [ some_context ]( const auto & p_http ) {
             ...
```

The callback receives as parameter a const reference on the HTTP object.

# Retrieving the response

Once the request is finished, you can use:
- `get_code()`:         get HTTP response code, or one of the libcurl error codes
- `get_body()`:         get response body
- `get_headers()`:      get all response headers as a map with case insensitive keys
- `get_content_type()`: get the received `Content-Type` header
- `get_redirect_url()`: get the received `Location` header

Note: do not call these accessors (or use previously obtained references) while
the request is running, as this may cause undefined behavior.

Or if a `std::future` was used, `get()` returns a structure with the same information.

Note: because the data are copied into the promise, this approach may be slightly
less efficient than the other methods.

## REST

If `curlev` was compiled with RapidJSON or nlohmann/json, the `HTTP` object
has an extra method `get_json()`, which retrieve either a `rapidjson::Document`
or a `nlohmann::json`.

This method returns false if the parsing of the received by fails.

# Aborting a request

A request can be aborted while running by calling the `abort()` method.

# Examples

Assuming the following code:

```cpp
#include <curlev/http.hpp>
using namespace curlev;
```

Starting the ASync instance:
```cpp
ASync async;
async.start();
```

A simple GET with parameters in the query string:
```cpp
auto http = HTTP::create( async );
auto code =
    http->GET( "http://www.httpbin.org/get",
               { { "name", "Alice" }, { "role", "admin" } } )
        .exec()
        .get_code();
std::cout << code << " " << http->get_body() << std::endl;
```

A POST with `application/x-www-form-urlencoded` parameters.
```cpp
auto http = HTTP::create( async );
auto code =
    http->POST( "http://www.httpbin.org/post",
                { { "name", "Alice" }, { "role", "admin" } } )
        .exec()
        .get_code();
std::cout << code << " " << http->get_body() << std::endl;
```

An asynchronous example:
```cpp
  auto http = HTTP::create( async );
  http->GET( "http://www.httpbin.org/get",
              { { "name", "Alice" }, { "role", "admin" } } )
      .start();
  //
  some_lengthy_function();
  //
  auto code = http->join().get_code();
  std::cout << code << " " << http->get_body() << std::endl;
```

Using call back on a detached request:
```cpp
  ...
  auto http = HTTP::create( async );
  http->GET( "http://www.httpbin.org/get",
             { { "name", "Alice" }, { "role", "admin" } } )
      .start( [ some_context ]( const auto & p_http )
              { some_context.do( p_http.get_code(), p_http.get_body() ); } );
}
```

Posting a MIME document:
```cpp
std::string data = read_file( "alice.jpg" );
auto        code =
    http->POST( "http://www.httpbin.org/post" )
        .add_mime_parameters( { mime::parameter{ "name", "Alice" },
                                mime::parameter{ "role", "admin" },
                                mime::file     { "picture", "img/69073875.jpg", "image/jpeg", "alice.jpg" } } )
        .exec()
        .get_code();
std::cout << code << " " << http->get_body() << std::endl;
```

A complex request:
```cpp
auto http = HTTP::create( async );
auto code =
    http->POST( "http://www.httpbin.org/get" )
        .add_body_parameters ( { { "name", "Alice" },
                                 { "role", "admin" } } )
        .add_query_parameters( { { "from", "12" },
                                 { "to"  , "23" } } )
        .add_headers         ( { { "Accept"         , "text/html" },
                                 { "Accept-Language", "en-US"     } } )
        .authentication      ( "mode=basic,user=joe,secret=abc123" )
        .options             ( "follow_location=1,insecure=1,timeout=120000" )
        .exec()
        .get_code();
std::cout << code << " " << http->get_body() << std::endl;
```
