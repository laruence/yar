# Yar - Yet Another RPC framework for PHP

[![Build status](https://ci.appveyor.com/api/projects/status/syhw33wlt2nad64i/branch/master?svg=true)](https://ci.appveyor.com/project/laruence/yar/branch/master) [![Build Status](https://github.com/laruence/yar/workflows/integrate/badge.svg)](https://github.com/laruence/yar/actions?query=workflow%3Aintegrate)

Light, concurrent RPC framework for PHP (see also: [Yar C framework](https://github.com/laruence/yar-c), [Yar Java framework](https://github.com/weibocom/yar-java), [Lua Yar framework](https://github.com/fangfengxiang/lua-yar))

## Requirement

- PHP 7.0+ (master branch)
- PHP 5.2+ ([php5 branch](https://github.com/laruence/yar/tree/php5))
- Curl
- Json
- Msgpack (Optional)

## Introduction

Yar is an RPC framework which provides a simple and easy way to do communication between PHP applications. It also offers the ability to make multiple calls to remote services concurrently.

## Features

- Fast, easy, simple
- Concurrent RPC calls
- Multiple data packagers supported (php, json, msgpack built-in)
- Multiple transfer protocols supported (HTTP, HTTPS, TCP)
- Detailed debug information

## Install

### Install via PECL

Yar is a PECL extension, simply install it by:

```bash
$ pecl install yar
```

### Compile from source

```bash
$ /path/to/phpize
$ ./configure --with-php-config=/path/to/php-config/
$ make && make install
```

Available configure options:

```bash
--with-curl=DIR
--enable-msgpack / --disable-msgpack
--enable-epoll / --disable-epoll (requires Yar 2.1.2)
```

### Install Yar with msgpack

1. Install msgpack extension for PHP:

```bash
$ pecl install msgpack
```

Or for Ubuntu:

```bash
$ apt-get install msgpack-php
```

Or, get the source from GitHub: https://github.com/msgpack/msgpack-php

2. Configure with msgpack enabled:

```bash
$ /path/to/phpize
$ ./configure --with-php-config=/path/to/php-config/ --enable-msgpack
$ make && make install
```

## Runtime Configuration

| INI Setting | Default | Description |
|---|---|---|
| `yar.timeout` | `5000` | Timeout in milliseconds |
| `yar.connect_timeout` | `1000` | Connection timeout in milliseconds |
| `yar.packager` | `"php"` (or `"msgpack"` if built with `--enable-msgpack`) | One of `"php"`, `"json"`, `"msgpack"` |
| `yar.debug` | `Off` | Enable debug mode |
| `yar.expose_info` | `On` | Whether to output the API info page for GET requests |
| `yar.content_type` | `"application/octet-stream"` | Content-Type sent in responses |
| `yar.allow_persistent` | `Off` | Whether to allow persistent connections |

> **Note**: `yar.connect_timeout` is in milliseconds. Prior to 1.2.1 it was measured in seconds.

## Constants

```php
YAR_VERSION
YAR_OPT_PACKAGER
YAR_OPT_PERSISTENT
YAR_OPT_TIMEOUT
YAR_OPT_CONNECT_TIMEOUT
YAR_OPT_HEADER          // Since 2.0.4
YAR_OPT_PROXY           // Since 2.2.0
YAR_OPT_RESOLVE         // Since 2.1.0
YAR_OPT_PROVIDER        // Since 2.3.0
YAR_OPT_TOKEN           // Since 2.3.0
```

## Server

### HTTP Server

It's very easy to set up a Yar HTTP RPC Server:

```php
<?php
class API
{
    /**
     * The doc info will be generated automatically into the service info page.
     * @params
     * @return
     */
    public function some_method($parameter, $option = "foo")
    {
    }

    protected function client_can_not_see()
    {
    }
}

$service = new Yar_Server(new API());
$service->handle();
```

Usual RPC calls are issued as HTTP POST requests.

If an HTTP GET request is issued to the URI (access the API address directly via a browser), the service info page (generated from the doc comments above) will be returned:

![yar service info page](https://github.com/laruence/laruence.github.com/raw/master/yar_server.png)

### Custom Server Info

Since 2.3.0, you can customise the output of the service info page by defining a `__info` magic method:

```php
<?php
class API
{
    protected function __info($markup)
    {
        return "Hello world";
    }
}
```

Then if an HTTP GET request is issued, "Hello world" will be returned instead.

### Authentication

Since 2.3.0, Yar allows the server to authenticate client requests via `Provider` / `Token` fields in the header. To enable this, define a protected magic method named `__auth` on the server side:

```php
<?php
class API
{
    protected function __auth($provider, $token)
    {
        return verify($provider, $token);
    }
}
```

> **Note**: `__auth` must always be defined as `protected`.

If `__auth` is defined, it will be called at the very beginning of every request:

- If `__auth` returns `true`, the request proceeds.
- Otherwise, the request is terminated with an "authentication failed" error.

On the client side, specify the provider and token via:

```php
<?php
$client->setOpt(YAR_OPT_PROVIDER, "provider");
$client->setOpt(YAR_OPT_TOKEN, "token");
$client->call();
```

## Client

### Synchronous Call

```php
<?php
$client = new Yar_Client("http://host/api/");

/* the following setOpt calls are optional */
$client->setOpt(YAR_OPT_CONNECT_TIMEOUT, 1000);

$client->setOpt(YAR_OPT_HEADER, ["hd1: val", "hd2: val"]); // Custom headers, Since 2.0.4

/* call remote service */
$result = $client->some_method("parameter");
```

### Concurrent Call

Yar supports sending multiple calls concurrently and collecting the results via a callback loop.

Each `callback` receives two arguments:
- `$retval` — the return value of the remote method
- `$callinfo` — an array with call metadata

Each `error_callback` receives three arguments:
- `$type` — the error type constant
- `$error` — the error message
- `$callinfo` — an array with call metadata

```php
<?php
function callback($retval, $callinfo)
{
    var_dump($retval);
}

function error_callback($type, $error, $callinfo)
{
    error_log($error);
}

Yar_Concurrent_Client::call("http://host/api/", "some_method", ["parameters"], "callback");

// If no callback is specified, the callback in loop() will be used
Yar_Concurrent_Client::call("http://host/api/", "some_method", ["parameters"]);

// This server accepts json packager
Yar_Concurrent_Client::call("http://host/api/", "some_method", ["parameters"],
    "callback", "error_callback", [YAR_OPT_PACKAGER => "json"]);

// Custom timeout
Yar_Concurrent_Client::call("http://host/api/", "some_method", ["parameters"],
    "callback", "error_callback", [YAR_OPT_TIMEOUT => 1]);

// Send all requests. The error_callback is optional.
Yar_Concurrent_Client::loop("callback", "error_callback");
```

### Persistent Connections

Since 2.1.0, if `YAR_OPT_PERSISTENT` is set to `true`, Yar will use HTTP keep-alive to speed up repeated calls to the same address. The connection is released at the end of the PHP request lifecycle.

```php
<?php
$client = new Yar_Client("http://host/api/");
$client->setOpt(YAR_OPT_PERSISTENT, 1);

$result = $client->some_method("parameter");

/* The following calls will speed up due to keep-alive */
$result = $client->some_other_method1("parameter");
$result = $client->some_other_method2("parameter");
$result = $client->some_other_method3("parameter");
```

### Custom Hostname Resolution

Since 2.1.0, when running over HTTP, `YAR_OPT_RESOLVE` can be used to override hostname resolution.

```php
<?php
$client = new Yar_Client("http://host/api/");

$client->setOpt(YAR_OPT_RESOLVE, ["host:80:127.0.0.1"]);

/* call goes to 127.0.0.1 instead of the DNS-resolved host */
$result = $client->some_method("parameter");
```

### Using an HTTP Proxy

Since 2.2.1, when running over HTTP, `YAR_OPT_PROXY` can be used to route calls through an HTTP proxy (e.g. Fiddler or Charles).

```php
<?php
$client = new Yar_Client("http://host/api/");

$client->setOpt(YAR_OPT_PROXY, "127.0.0.1:8888"); // HTTP proxy, Since 2.2.0

/* call is routed through the proxy */
$result = $client->some_method("parameter");
```

## Protocols

Yar is not only designed for PHP — all RPC requests and responses are transferred as binary data streams.

### Yar Header

Key messages are exchanged via a struct called "Yar Header":

```c
#ifdef PHP_WIN32
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct _yar_header {
    uint32_t       id;            // transaction id
    uint16_t       version;       // protocol version
    uint32_t       magic_num;     // default is: 0x80DFEC60
    uint32_t       reserved;
    unsigned char  provider[32];  // request from who
    unsigned char  token[32];     // request token, used for authentication
    uint32_t       body_len;      // request body length
}
#ifndef PHP_WIN32
__attribute__ ((packed))
#endif
yar_header_t;
#ifdef PHP_WIN32
#pragma pack(pop)
#endif
```

### Packager Header

Yar supports multiple packager protocols via a `char[8]` identifier placed before the header struct. This indicates which packager was used to encode the body.

### Request

When a client makes an RPC request, the request body is sent as an array (in PHP):

```php
<?php
[
    "i" => '', // transaction id
    "m" => '', // the method being called
    "p" => [], // parameters
]
```

### Response

When a server responds, the response body is also sent as an array (in PHP):

```php
<?php
[
    "i" => '', // transaction id
    "s" => '', // status
    "r" => '', // return value
    "o" => '', // output
    "e" => '', // error or exception
]
```

## License

[PHP-3.01](https://www.php.net/license/3_01.txt)
