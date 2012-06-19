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

[![Service_Info](https://github.com/laruence/laruence.github.com/raw/master/yar_server.png)]


