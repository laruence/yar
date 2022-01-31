--TEST--
Check for yar client with non yar server
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php 
include "yar.inc";

yar_server_start(<<<'PHP'
<?php
echo "hello world";
PHP
);

$client = new Yar_Client(YAR_API_ADDRESS);

try {
    $client->fatal();
} catch (Yar_Client_Protocol_Exception $exceptin) {
    var_dump($exceptin->getMessage());
}

--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECT--
string(39) "malformed response header 'hello world'"
