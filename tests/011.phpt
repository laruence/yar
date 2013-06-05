--TEST--
Check for yar server __auth
--SKIPIF--
<?php 
echo "skip"; //auth is not supported any more
if (!extension_loaded("yar")) {
    print "skip";
}
include "skip.inc";
?>
--FILE--
<?php 
include "yar.inc";

$client = new Yar_Client(YAR_API_ADDRESS . 'auth.php');

try {
    var_dump($client->normal("dummy"));
} catch (Yar_Server_Exception $e) {
    var_dump($e->getMessage());
}


$client = new Yar_Client("http://dummy:dummy@" . YAR_API_HOST . YAR_API_URI . 'auth.php');
var_dump($client->normal("dummy"));
?>
--EXPECTF--
string(39) "Service_Provider::__auth() return false"
string(3) "3.6"
