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

yar_server_start();

$client = new Yar_Client(YAR_API_ADDRESS . 'rest.php');

try {
    $client->fatal();
} catch (Yar_Client_Protocol_Exception $exceptin) {
    var_dump($exceptin->getMessage());
}

--EXPECT--
string(39) "malformed response header 'hello world'"
