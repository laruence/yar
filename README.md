# Yar - Yet Another RPC framework for PHP

Light, concurrent RPC framework for PHP(c, java etc will be supported soon)

This extension is under developing, please do not use this in a product

## Requirement
- PHP >= 5.2
- Curl
- Json

## Introduce

Yar is a RPC framwork which aim to provide a simply and easy way to communication between PHP applications

It also provide ability to call remote services concurrently.

## Features
- Fast, Easy, Simply
- Multi packager protocol supported (php, json, msgpack built-in)
- Concurrent RPC call
- Authentication

## Server

It's very easy to setup a Yar RPC Server

    <?php
        class API {
        /**
         * the doc info will be generated automaticly into service info page.
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
they if a GET request to this api uri,  a service info will be auto generated:

(https://github.com/laruence/laruence.github.com/raw/master/yar_server.png)


## Client

### Normal call
It's very easy for a PHP client to call remote RPC:

    <?php
    $client = new Yar_Client("http://host/api/");
    $result $client->api("parameter);
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


