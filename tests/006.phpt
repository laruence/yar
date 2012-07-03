--TEST--
Check for yar client with exception
--SKIPIF--
<?php if (!extension_loaded("yar") || version_compare(PHP_VERSION, '5.4.0', '<')) print "skip"; ?>
--FILE--
<?php 
include "yar.inc";
yar_server_start(<<<'DOC'
class Server {

    /**
     * hello world
     * @param string $name
     * @return boolean
     */
    public function helloworld($name) {
       return "yar";
    }

    public function exception() {
       throw new Exception("dummy");
    }
}

$server = new Yar_Server(new Server());
$server->handle();
DOC
, false);

$client = new Yar_Client(YAR_SERVER_HTTP_ADDRESS);

function callback($return, $callinfo) {
    echo $return;
}

Yar_Concurrent_Client::call(YAR_SERVER_HTTP_ADDRESS, "helloworld", array("xxx")); 
Yar_Concurrent_Client::call(YAR_SERVER_HTTP_ADDRESS, "helloworld", array("xxx"), "callback"); 
Yar_Concurrent_Client::call(YAR_SERVER_HTTP_ADDRESS, "helloworld", array("xxx"), "callback"); 
Yar_Concurrent_Client::loop();
--EXPECT--
yaryaryar
