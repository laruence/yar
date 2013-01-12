--TEST--
Check for yar client with exception
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
include "skip.inc";
?>
--FILE--
<?php 
include "yar.inc";

$client = new Yar_Client(YAR_API_ADDRESS);

try {
$client->fatal();
} catch (Yar_Server_Exception $exceptin) {
   var_dump($exceptin->getMessage());
}

--EXPECT--
string(47) "call to undefined api Service_Provider::fatal()"
