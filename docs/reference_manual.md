curlev - reference manual
=========================

All classes and functions are inside the namespace `curlev`.
The only headers to include are `curlev/http.hpp`,
or `curlev/smtp.hpp`.

# Quickstart guide (for HTTP)

 - Include <curlev/http.hpp>
 - Instantiate a global `ASync` object
 - To do a request:
   - create an `HTTP` object using `HTTP::create()`
   - call one of `GET()`, `DELETE()`, `POST()`, `PUT()` or `PATCH()` (or `REST()`)
   - optionally call `add_headers()`, `set_...()`, `options()`...
   - call `exec()` (synchronous), `start()`/`join()` (asynchronous), or `launch()` (future)
   - call the `get_...()` methods
- Notes:
  - if the callback in `start()` is set, it must be as fast as possible
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

The default configuration of the various protocol instances can be set in `ASync` using
the three methods `options()`, `authentication()` and `certificates()`.

### Authentication

The string expected by `authentication()` is a key-value comma
separated string with the following keys available:

| Name   | Comment                                         | libcurl options
|--------|-------------------------------------------------|--------------------
| mode   | "none", "basic", "digest" or "bearer"           | CURLAUTH_NONE, CURLAUTH_BASIC, CURLAUTH_DIGEST or CURLAUTH_BEARER
| user   | for "basic" and "digest" modes only: user login | CURLOPT_USERNAME
| secret | password or token                               | CURLOPT_PASSWORD or CURLOPT_XOAUTH2_BEARER

For example:
- mode=basic,user=joe,secret=abc123
- mode=bearer,secret=ABCDEFHIJKLMNOQRSTUVWXYZ

### Options

The string expected by `options()` is a key-value comma
separated string with the following keys available:

| Name               | Default | Unit         | Comment                             | libcurl options
|--------------------|---------|--------------|-------------------------------------|---------------------
| accept_compression | 1       | 0 or 1       | activate compression                | CURLOPT_ACCEPT_ENCODING
| connect_timeout    | 30000   | milliseconds | connection timeout                  | CURLOPT_CONNECTTIMEOUT_MS
| cookies            | 0       | 0 or 1       | receive and resend cookies          | CURLOPT_COOKIEFILE
| follow_location    | 0       | 0, 1, 2, 3   | follow HTTP 3xx redirects           | CURLOPT_FOLLOWLOCATION (ALL, OBEYCODE, FIRSTONLY)
| insecure           | 0       | 0 or 1       | disables certificate validation     | CURLOPT_SSL_VERIFYHOST and CURLOPT_SSL_VERIFYPEER
| maxredirs          | 5       | count        | maximum number of redirects allowed | CURLOPT_MAXREDIRS
| proxy              |         | string       | the SOCKS or HTTP URl to a proxy    | CURLOPT_PROXY
| rcpt_allow_fails   | 0       | 0 or 1       | continue if some recipients fail    | CURLOPT_MAIL_RCPT_ALLOWFAILS
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
- follow_location:    see modes in https://curl.se/libcurl/c/CURLOPT_FOLLOWLOCATION.html

### Certificates

The string expected by `certificates()` is a key-value comma
separated string with the following keys available:

| Connection | Usage    | Key               | Comment                                        | libcurl options
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

# HTTP requests

`libcurl` supports HTTP/1, HTTP/2 and HTTP/3.
URL are prefixed by schemas `http://` or `https://`.

## Creating an instance

The static method `create()` returns a `shared_ptr` on the new object. Using a `shared_ptr`
and its reference counter allows to have an object that can continues to run detached from
the creation context.

```cpp
auto http = HTTP::create( async );  // async is the instance of ASync
```

## Building the request

On the `HTTP` object, you first have to call one of the HTTP methods.
This will restart a new request sessions.
Available methods are `GET()`, `DELETE()`, `POST()`, `PUT()` and `PATCH()`.

`GET()` and `DELETE()` are requests without body and expect an URL and
an optional map of parameters to send in the query string.

`DELETE()`, `POST()`, `PUT()` and `PATCH()` are requests (usually) with
a body. These methods expect an URL and an optional map of parameters
to send in the query string. One of the following method must then be
called to specify the body:

1. set_body           to pass a raw body
2. set_parameters     to add body parameters as `application/x-www-form-urlencoded`
3. set_mime           to pass MIME parts

Then, if needed, one or several of the following configuration methods
are available:

- `add_headers( headers )`:         add headers
- `authentication( auth_string )`:  set authentication
- `options( opt_string )`:          set options
- `certificates( cert_string )`:    set SSL certificates

Examples:

```cpp
auto http = HTTP::create( async );
http->GET( "http://www.httpbin.org/get", { { "a", "1" }, { "b", "2" } } )
    .exec();
std::cout << http->get_code() << " " << http->get_body() << std::endl;
```

```cpp
auto http = HTTP::create( async );
http->POST( "http://www.httpbin.org/post" )
    .set_parameters( { { "a", "1" }, { "b", "2" } } )
    .exec();
std::cout << http->get_code() << " " << http->get_body() << std::endl;
```

### REST

If `curlev` was compiled with RapidJSON or nlohmann/json, the `HTTP` object
has an extra method `REST()` expecting an URL, a verb,
a JSON object (a `rapidjson::Document` or a `nlohmann::json`),
and an optional map of parameters to send in the query string.

Example:
```cpp
nlohmann::json payload = nlohmann::json::parse( R"({ "a": "1", "b": "2" })" );
//
auto http = HTTP::create( async );
http->REST( "http://www.httpbin.org/post", "POST", payload ).exec();
std::cout << http->get_code() << " " << http->get_body() << std::endl;
```

### Adding headers and parameters

The optional query parameters of the HTTP methods,
and the `add_headers()` and `set_parameters` methods
expect an unordered map of keys/parameters and values
known as `curlev::key_values`.

```cpp
add_headers( { { "Accept"         , "text/html" },
               { "Accept-Language", "en-US"     } } ).
set_parameters( { { "p1", "1" },
                  { "p2", "2" } } )
```

### Adding MIME parts

The `set_mime` method expects a vector of MIME parts:

- `mime::parameter` to add a simple name/value parameter
  - fields are: `name`, `value`
```cpp
set_mime( { mime::parameter{ "p1", "1" },
            mime::parameter{ "p2", "2" } } )
```

- `mime::data`      to add data part, with an optional Content-Type and remote file name
  - fields are: `name`, `data`, `content_type`, `filename`
```cpp
set_mime( { mime::parameter{ "p1", "1" },
            mime::data     { "p2", "Hello, world!", "text/plain", "f.txt" } } )
```

- `mime::file`      to read data from a file as a part, with an optional Content-Type and remote file name
  - fields are: `name`, `filedata`, `content_type`, `filename`
  - default `filename` is the base name of `filedata`
```cpp
set_mime( { mime::parameter{ "p1", "1" },
            mime::file     { "p2", "/tmp/uZ7hHC2", "text/plain", "f.txt" } } )
```

### Adding authentication, options and certificates

The same three methods `options()`, `authentication()` and `certificates()` present in
`ASync` are also present in `HTTP`, and allows to override the default configuration.

They use the same syntax.

## Executing the request

Once the request is ready, it can be started using `start()`.
The request then runs asynchronously. This method accepts a callback function
that will be called once the request is finished, successfully or not.
Then call `join()` to wait for asynchronous request completion.

Or the request can be started using the convenient method `exec()`,
which executes the request synchronously (`start()` + `join()`).

Or a `std::future` can be retrieved using `launch()`.

While a request is running, all methods except `join()` are ignored
(as a side effect `exec()` behaves like `join()`).

The `HTTP` object can be configured to retry automatically if the request
fails, if there no risk that the request has been executed (it retries
on connection error, not on timeout). The method to use is `set_retries()`.

### Callback

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

## Retrieving the response

Once the request is finished, you can use:
- `get_code()`:         get HTTP response code, or one of the libcurl error codes
- `get_body()`:         get response body
- `get_headers()`:      get all response headers as a map with case insensitive keys
- `get_content_type()`: get the received `Content-Type` header
- `get_redirect_url()`: get the received `Location` header

Note: do not call these accessors (or use previously obtained references) while
the request is running, as this may cause undefined behavior.

Or if a `std::future` was used, `get()` returns a structure with the same information.

Note: because the response is moved to the `std::future`, it is
not available anymore in the `HTTP` object.

Note: the maximal received response size is set y default to 2MB, but can
be changed by using `maximal_response_size()` in `HTTP`.

Note: headers are available a case insensitive unsorted map, allowing
to retrieve a header independently of its case:

```cpp
  auto ct = http->get_headers().at( "Content-Type" );
  auto ct = http->get_headers().at( "content-type" );
```

### REST

If `curlev` was compiled with RapidJSON or nlohmann/json, the `HTTP` object
has an extra method `get_json()`, which retrieve either a `rapidjson::Document`
or a `nlohmann::json`.

This method returns false if the parsing of the received by fails.

```cpp
nlohmann::json json;

if ( http->get_json( json ) )
  std::cout << json["user"]["name"] << std::endl;
```

## Aborting a request

A request can be aborted while running by calling the `abort()` method.

## Examples

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
    http->POST( "http://www.httpbin.org/post" )
        .set_parameters( { { "name", "Alice" }, { "role", "admin" } } )
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
        .set_mime( { mime::parameter{ "name", "Alice" },
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
    http->POST( "http://www.httpbin.org/get", { { "from", "12" },
                                                { "to"  , "23" } } )
        .set_parameters ( { { "name", "Alice" },
                            { "role", "admin" } } )
        .add_headers         ( { { "Accept"         , "text/html" },
                                 { "Accept-Language", "en-US"     } } )
        .authentication      ( "mode=basic,user=joe,secret=abc123" )
        .options             ( "follow_location=1,insecure=1,timeout=120000" )
        .exec()
        .get_code();
std::cout << code << " " << http->get_body() << std::endl;
```

# SMTP requests

`libcurl` supports SMTP and SMTPS protocols for sending emails.
URLs are prefixed by the schemas `smtp://` or `smtps://`.

## Creating an instance

The static method `create()` returns a `shared_ptr` to a new `SMTP` object.
As with HTTP, using a `shared_ptr` allows the object to run detached from
the creation context.

```cpp
auto smtp = SMTP::create( async );  // async is the instance of ASync
```

## Building the request

On the `SMTP` object, you must first call the `SEND()` method
to start a new email request session:

Then you can either add a MIME body or a raw body using `set_mime` or
`set_body`.

If adding a MIME body, it is possible to  add custom headers
using `add_headers()` which expects an unordered map of keys
and values known as `curlev::key_values`.

```cpp
smtp->add_headers( { { "Priority", "urgent" } } );
```

### Address format

The `smtp::address` struct accepts email addresses with or without display names:

- `Mary Smith <mary@x.test>`
- `"Mary Smith" <mary@x.test>`
- `<mary@x.test>`
- `mary@x.test`

`smtp::address` also accepts the recipient mode (To, Cc, or Bcc).
This is only used with the [1] form of SEND since this is part of
the RFC5322 body message, and not of SMTP.

```cpp
auto a = smtp::address( "<boss@nil.test>", smtp::address::Mode::cc );
```

### MIME document

`SEND()` expects a vector of MIME parts: either a list of `mime::data`
and `mime::file`, or `mime::alternatives` containing `mime::data`, or
a mix.

Typical configurations are:

An email without attachment, with a plain text and an HTML version of a message.
```cpp
{ mime::alternatives{
    mime::data{ "", "Hello"       , "text/plain", "" },
    mime::data{ "", "<b>Hello</b>", "text/html" , "" }
  }
 }
```

An email with attachment, with a plain text and an HTML version of a message.
```cpp
{ mime::alternatives{
    mime::data{ "", "Hello"       , "text/plain", "" },
    mime::data{ "", "<b>Hello</b>", "text/html" , "" }
  }
  mime::file{ "", "/tmp/content.dat", "text/plain" , "a.txt"  }
}
```

An email with a plain text message and an attachment.
```cpp
{ mime::data{ "", "Hello"           , "text/plain", ""        },
  mime::file{ "", "/tmp/content.dat", "text/plain" , "a.txt"  }
}
```

## Authentication, options, and certificates

The same methods as in `ASync` are available to configure authentication, options,
and certificates: `options()`, `authentication()` and `certificates()`.

They use the same syntax.

## Executing the request

Once the request is ready, it can be started using `start()` (asynchronous),
`exec()` (synchronous), or `launch()` (future):

See HTTP's `Executing the request` for more details.
Executing the request](##-Executing-the-request)

## Retrieving the response

After the request completes, you can retrieve the SMTP response code:

```cpp
long code = smtp->get_code();
```

Or, if using `launch()`, from the returned `response` struct:
```cpp
auto resp = smtp->launch().get();
long code = resp.code;
```

## Example

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

Sending an email synchronously:
```cpp
auto smtp = SMTP::create( async );
auto code = smtp->SEND(
    "smtps://smtp.example.com:465",
    smtp::address("John Smith <john.smith@example.com>"),
    { smtp::address("alice@example.org") },
    "Test email",
    { mime::data{ "", "Hello, Alice!", "text/plain", "" } }
  )
  .authentication("mode=basic,user=john.smith,secret=yourpassword")
  .exec()
  .get_code();
std::cout << code << std::endl;
```

To send a raw email (RFC5322 message):
```cpp
smtp->SEND(
    "smtp://smtp.example.com:25",
    smtp::address("john.smith@example.com"),
    { smtp::address("alice@example.org") },
    "From: John Smith <john.smith@example.com>\r\n"
    "To: Alice <alice@example.org>\r\n"
    "Subject: Test\r\n"
    "\r\n"
    "Hello, world!"
).exec();
```
