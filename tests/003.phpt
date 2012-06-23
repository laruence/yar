--TEST--
Check for yar client
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
       echo "Hello World, " , $name , "\n";
       return TRUE;
    }
}

$server = new Yar_Server(new Server());
$server->handle();
DOC
, false);

$client = new Yar_Client(YAR_SERVER_HTTP_ADDRESS);
var_dump($client->helloworld("yar"));
?>
--EXPECT--
Hello World, yar
bool(true)
