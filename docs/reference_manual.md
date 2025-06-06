curlev - manual
===============

All classes and functions are inside the namespace `curlev`.

# Starting

The `libcurl` and `libuv` libraries must be started before any other operations,
usually when the application starts. A single instance of `ASync` must be instantiated
and started using:

```cpp
m_async.start();
```

Similarly they must be stop when the application stops using:

```cpp
m_async.stop();
```

`stop()` waits for all pending requests to finish before returning.
If a timeout occurs (30s bby default), it returns `true`.
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

# Creating an HTTP instance

The static method `create()` returns a `shared_ptr` on the new object. Using a `shared_ptr`
and its reference counter allows to have an object that can continues to run detached from
the creation context.

```cpp
auto http = HTTP::create( m_async );  // async is the instance of ASync
```

# Building the request

On the HTTP object, you have to call one of the HTTP methods `GET()`, `DELETE()`,
`POST()`, `PUT()` or `PATCH()`.

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
- `clear()`:                        reset the HTTP method and all options

The `add_mime_parameters` method expects a vector of parameters and files
to place in the MIME document that will be sent as the body of the request:

- `HTTP::t_mime_parameter{ key, value }` are simple standard parameters
- `HTTP::t_mime_file{ key, content_type, data, filename }` are attachments where
  the uploaded data is associated to a file name (the data is not read from
  the file, you have to provide the data to send). Even if data is a std::string
  it can hold binary data such as images.

# Adding authentication

The string expected by the `authentication()` is a key-value comma
separated string with the following keys available:

| Name   | Comment
|--------|-----------------------------
| mode   | basic, digest or bearer
| user   | for basic and digest only: user login
| secret | password or token

For example:
- mode=basic,user=joe,secret=abc123
- mode=bearer,secret=ABCDEFHIJKLMNOQRSTUVWXYZ

# Setting options

The string expected by the `options()` is a key-value comma
separated string with the following keys available:

| Name             | Default | Unit   | Comment
|------------------|---------|--------|-------------------------
| connect_timeout  | 30      | second | connection timeout
| follow_location  | 0       | 0 or 1 | follow HTTP 3xx redirects
| insecure         | 0       | 0 or 1 | disables certificate validation
| timeout          | 30      | second | receive data timeout
| verbose          | 0       | 0 or 1 | debug log on console
| cookies          | false   | 0 or 1 | receive and resend cookies

For example:
- follow_location=1,timeout=5
- insecure=1
  
# Executing the request

Once the request is ready, it has to be started using `start()`.
The request run then asynchronously. This method accepts a callback function
that will be called once the request is finished, successfully or not.
Then call `join()` to wait for asynchronous request completion.

The convenient method `exec()` execute the request synchronously.

By default the callback is invoked from a separate thread. This prevents
the main IO thread to be blocked while processing the callback but
add a small extra overhead. If the callback is known to be very fast,
it is possible to invoke it directly from the IO thread by changing
the default mode using `threaded_callback()` method of the HTTP instance:

```cpp
http->GET( "http://www.httpbin.org/get",
           { { "name", "Alice" }, { "role", "admin" } } )
     .threaded_callback( false )
     .start( [ some_context ]( const auto & p_http ) {
             ...
```

While a request is running, all methods except `join()` and `exec()` are ignored.

# Retrieving the response

Once the request is finished, the
- `get_code()`:         get HTTP response code, or one of the libcurl error code
- `get_body()`:         get response body
- `get_headers()`:      get all response headers as a map
- `get_content_type()`: get the received `Content-Type` header
- `get_redirect_url()`: get the received `Location header` header

# Aborting a request

A request can be aborted while running by calling the `abort()` method.

# Examples

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
  auto code = join().get_code();
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
    http->POST( "http://www.httpbin.org/get" )
        .add_mime_parameters( { ( HTTP::t_mime_parameter{ "name", "Alice" } ),
                                ( HTTP::t_mime_parameter{ "role", "admin" } ),
                                ( HTTP::t_mime_file     { "picture", "image/jpeg", data, "alice.jpg" } ) } )
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
        .options             ( "follow_location=1,insecure=1,timeout=120" )
        .exec()
        .get_code();
std::cout << code << " " << http->get_body() << std::endl;
```
