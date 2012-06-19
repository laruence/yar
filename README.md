# Yar - Yet Another RPC framework for PHP

Light, concurrent RPC framework for PHP(c, java etc will be supported soon)

## Requirement
- PHP >= 5.2
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
- Authentication
- Detailed debug informations

## Server

It's very easy to setup a Yar HTTP RPC Server

    <?php
        class API {
        /**
         * the doc info will be generated automatically into service info page.
         * @params 
         * @return
         */
        public function api($parameter, $option = "foo") {
        }
    
        protected function client_can_not_see() {
        }
    }

    $service = new Yar_Server(new API());
    $service->handle();
    ?>
Usual RPC calls will be issued as HTTP POST requests. If a HTTP GET request is issued to the uri, the service information (commented section above) will be printed on the page:

[![](https://github.com/laruence/laruence.github.com/raw/master/yar_server.png)]


## Client

### Synchronous call
It's very easy for a PHP client to call remote RPC:

    <?php
    $client = new Yar_Client("http://host/api/");
    $result = $client->api("parameter);
    ?>
### Concurrent call
    <?php
    function callback($retval, $sequence_id, $method_name, $uri) {
         var_dump($retval);
    }
    
    Yar_Concurrent_Client::call("http://host/api/", "api", array("parameters"), "callback");
    Yar_Concurrent_Client::call("http://host/api/", "api", array("parameters"), "callback");
    Yar_Concurrent_Client::call("http://host/api/", "api", array("parameters"), "callback");
    Yar_Concurrent_Client::call("http://host/api/", "api", array("parameters"), "callback");
    Yar_Concurrent_Client::loop(); //send
    ?>
    
## Authentication

Setting up a server that requires authentication is also rather easy. You just have to declare a method called "_auth" in the server class.


    <?php
        class API {
        
        /**
         * if this method return false, then the rpc call will be denied
         */
        public function _auth($user, $password) {
        }
        
        /**
         * the doc info will be generated automatically into service info page.
         * @params 
         * @return
         */
        public function api($parameter, $option = "foo") {
        }
    
        protected function client_can_not_see() {
        }
    }

    $service = new Yar_Server(new API());
    $service->handle();
    ?>

Consequently, the client code should be change to :

    <?php
    $client = new Yar_Client("http://username:password@host/api/");
    $result = $client->api("parameter);
    ?>
