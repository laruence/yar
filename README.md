# Yar - Yet Another RPC framework for PHP
[![Build Status](https://secure.travis-ci.org/laruence/yar.png)](http://travis-ci.org/laruence/yar) [![Build status](https://ci.appveyor.com/api/projects/status/syhw33wlt2nad64i/branch/master?svg=true)](https://ci.appveyor.com/project/laruence/yar/branch/master)

Light, concurrent RPC framework for PHP(see also: [Yar C framework](https://github.com/laruence/yar-c), [Yar Java framework](https://github.com/weibocom/yar-java))

## Requirement
- PHP 7.0+  (master branch))
- PHP 5.2+  ([php5 branch](https://github.com/laruence/yar/tree/php5))
- Curl
- Json
- Msgpack (Optional)

## Introduction

Yar is a RPC framework which aims to provide a simple and easy way to do communication between PHP applications

It has the ability to concurrently call multiple remote services.

## Features
- Fast, Easy, Simple
- Concurrent RPC calls
- Multiple data packager supported (php, json, msgpack built-in)
- Multiple transfer protocols supported (http, https, TCP)
- Detailed debug informations

## Install

### Install Yar 
Yar is an PECL extension, thus you can simply install it by:
```
pecl install yar
```
### Compile Yar in Linux
```
$/path/to/phpize
$./configure --with-php-config=/path/to/php-config/
$make && make install
```
Available instructions to configure are
```
--with-curl=DIR
--enable(disable)-msgpack
--enable(disable)-epoll (require Yar 2.1.2)
```

### Install Yar with msgpack 
first you should install msgpack-ext
```
pecl install msgpack
```
or , you can get the github source here: https://github.com/msgpack/msgpack-php

then:
```
$phpize
$configure --with-php-config=/path/to/php-config/ --enable-msgpack
$make && make install
```
## Runtime Configure
- yar.timeout  //default 5000 (ms)
- yar.connect_timeout  //default 1000 (ms)
- yar.packager  //default "php", when built with --enable-msgpack then default "msgpack", it should be one of "php", "json", "msgpack"
- yar.debug    //default Off
- yar.expose_info // default On, whether output the API info for GET requests
- yar.content_type // default "application/octet-stream"
- yar.allow_persistent // default Off 

*NOTE* yar.connect_time is a value in milliseconds, and was measured in seconds in 1.2.1 and before.

## Constants
- YAR_VERSION
- YAR_OPT_PACKAGER
- YAR_OPT_PERSISTENT
- YAR_OPT_TIMEOUT
- YAR_OPT_CONNECT_TIMEOUT
- YAR_OPT_HEADER // Since 2.0.4
## Server

It's very easy to setup a Yar HTTP RPC Server
```php
<?php
class API {
    /**
     * the doc info will be generated automatically into service info page.
     * @params 
     * @return
     */
    public function some_method($parameter, $option = "foo") {
    }

    protected function client_can_not_see() {
    }
}

$service = new Yar_Server(new API());
$service->handle();
?>
```
Usual RPC calls will be issued as HTTP POST requests. If a HTTP GET request is issued to the uri, the service information (commented section above) will be printed on the page:

![yar service info page](https://github.com/laruence/laruence.github.com/raw/master/yar_server.png)


## Client
It's very simple for a PHP client to call remote RPC:
### Synchronous call
```php
<?php
$client = new Yar_Client("http://host/api/");
/* the following setopt is optinal */
$client->SetOpt(YAR_OPT_CONNECT_TIMEOUT, 1000);

$client->SetOpt(YAR_OPT_HEADER, array("hd1: val", "hd2: val"));  //Custom headers, Since 2.0.4

/* call remote service */
$result = $client->some_method("parameter");
?>
```
### Concurrent call
```php
<?php
function callback($retval, $callinfo) {
     var_dump($retval);
}

function error_callback($type, $error, $callinfo) {
    error_log($error);
}

Yar_Concurrent_Client::call("http://host/api/", "some_method", array("parameters"), "callback");
Yar_Concurrent_Client::call("http://host/api/", "some_method", array("parameters"));   // if the callback is not specificed, 
                                                                               // callback in loop will be used
Yar_Concurrent_Client::call("http://host/api/", "some_method", array("parameters"), "callback", "error_callback", array(YAR_OPT_PACKAGER => "json"));
                                                                               //this server accept json packager
Yar_Concurrent_Client::call("http://host/api/", "some_method", array("parameters"), "callback", "error_callback", array(YAR_OPT_TIMEOUT=>1));
                                                                               //custom timeout 
 
Yar_Concurrent_Client::loop("callback", "error_callback"); //send the requests, 
                                                           //the error_callback is optional
?>
```
### Persistent call
After Yar 2.1.0, if YAR_OPT_PERSISTENT is set to true, then Yar is able to use HTTP keep-alive to speedup repeated calls to a same address, the link will be released at the end of the PHP request lifecycle.
```php
<?php
$client = new Yar_Client("http://host/api/");
$client->SetOpt(YAR_OPT_PERSISTENT, 1);

$result = $client->some_method("parameter");

/* The following calls will speed up due to keep-alive */
$result = $client->some_other_method1("parameter");
$result = $client->some_other_method2("parameter");
$result = $client->some_other_method3("parameter");
?>
```
### Custom hostname resolving
After Yar 2.1.0, if Yar runs on HTTP protocol, YAR_OPT_RESOLVE could be used to define custom hostname resolving.
```php
<?php
$client = new Yar_Client("http://host/api/");

$client->SetOpt(YAR_OPT_RESOLVE, "host:80:127.0.0.1");

/* call goes to 127.0.0.1 */
$result = $client->some_method("parameter");
```
## Protocols
### Yar Header
   Since Yar will support multi transfer protocols, so there is a Header struct, I call it Yar Header
```C
#ifdef PHP_WIN32
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct _yar_header {
    uint32_t       id;            // transaction id
    uint16_t       version;       // protocol version
    uint32_t       magic_num;     // default is: 0x80DFEC60
    uint32_t       reserved;
    unsigned char  provider[32];  // reqeust from who
    unsigned char  token[32];     // request token, used for authentication
    uint32_t       body_len;      // request body len
}
#ifndef PHP_WIN32
__attribute__ ((packed))
#endif
yar_header_t;
#ifdef PHP_WIN32
#pragma pack(pop)
#endif
````
### Packager Header
   Since Yar also supports multi packager protocol, so there is a char[8] at the begining of body, to identicate which packager the body is packaged by.

### Request 
   When a Client request a remote server,  it will send a struct (in PHP):
```php
<?php
array(
   "i" => '', //transaction id
   "m" => '', //the method which being called
   "p" => array(), //parameters
)
```

### Server
When a server response a result,  it will send a struct (in PHP):
```php
<?php
array(
   "i" => '',
   "s" => '', //status
   "r" => '', //return value 
   "o" => '', //output 
   "e" => '', //error or exception
)
```
