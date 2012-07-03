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
       throw new Exception("dummy");
       return TRUE;
    }
}

$server = new Yar_Server(new Server());
$server->handle();
DOC
, false);

$client = new Yar_Client(YAR_SERVER_HTTP_ADDRESS);
try {
    var_dump($client->helloworld("yar"));
} catch (Yar_Server_Exception $e) {
    var_dump($e->getMessage());
    var_dump($e->getType());
}
?>
--EXPECT--
string(5) "dummy"
string(9) "Exception"
