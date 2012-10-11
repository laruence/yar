--TEST--
Check for yar client with exception
--SKIPIF--
<?php if (!extension_loaded("yar") || version_compare(PHP_VERSION, '5.4.0', '<')) print "skip"; ?>
--INI--
url_fopen=1
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
}

$server = new Yar_Server(new Server());
try {
    $server->handle();
} catch (Yar_Server_Exception $e) {
    echo $e->getCode() . " " . $e->getMessage();
}
DOC
, false, false);

var_dump(file_get_contents(YAR_SERVER_HTTP_ADDRESS));
--EXPECT--
string(39) "32 server info is not allowed to access"
