# Yar - Yet Another RPC framework for PHP
[![Build Status](https://secure.travis-ci.org/laruence/yar.png)](http://travis-ci.org/laruence/yar)

Light, concurrent RPC framework for PHP(c, java etc will be supported soon)

## Requirement
- PHP 5.2+
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
- Multiple transfer protocols supported (http implemented,  tcp/unix will be supported later)
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
- yar.timeout  //default 5
- yar.connect_timeout  //default 1
- yar.packager  //default "php", when built with --enable-msgpack then default "msgpack", it should be one of "php", "json", "msgpack"
- yar.debug    //default Off
- yar.expose_info // default On, whether output the API info for GET requests
- yar.content_type // default "application/octet-stream"
- yar.allow_persistent // default Off 


## Constants
- YAR_VERSION
- YAR_OPT_PACKAGER
- YAR_OPT_PERSISTENT
- YAR_OPT_TIMEOUT
- YAR_OPT_CONNECT_TIMEOUT

## Server

It's very easy to setup a Yar HTTP RPC Server 
### Http
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

### TCP
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

$service = new Yar_Server(new API(),"tcp://127.0.0.1/9090");
$service->handle();
?>
```

## Client
It's very easy for a PHP client to call remote RPC:

### Synchronous call
```php
<?php
$client = new Yar_Client("http://host/api/");
// Or $client = new Yar_Client("tcp://127.0.0.1/9090");

/* the following setopt is optinal */
$client->SetOpt(YAR_OPT_CONNECT_TIMEOUT, 1);

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
Yar_Concurrent_Client::call("http://host/api/", "some_method", array("parameters"), "callback", array(YAR_OPT_PACKAGER => "json"));
                                                                               //this server accept json packager
Yar_Concurrent_Client::call("http://host/api/", "some_method", array("parameters"), "callback", array(YAR_OPT_TIMEOUT=>1));
                                                                               //custom timeout 
 
Yar_Concurrent_Client::loop("callback", "error_callback"); //send the requests, 
                                                           //the error_callback is optional
?>
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
    unsigned int   id;            // transaction id
    unsigned short version;       // protocl version
    unsigned int   magic_num;     // default is: 0x80DFEC60
    unsigned int   reserved;
    unsigned char  provider[32];  // reqeust from who
    unsigned char  token[32];     // request token, used for authentication
    unsigned int   body_len;      // request body len
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
   Since Yar also supports multi packager protocl, so there is a char[8] at the begining of body, to identicate which packager the body is packaged by.

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
