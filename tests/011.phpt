--TEST--
Check for yar server __auth
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
$client = new Yar_Client(YAR_API_ADDRESS . '/auth.php');

try {
    var_dump($client->normal("dummy"));
} catch (Exception $e) {
    var_dump($e->getMessage());
}

$client = new Yar_Client("http://dummy:dummy@" . YAR_API_HOST . '/auth.php');
var_dump($client->normal("dummy"));

$client = new Yar_Client(YAR_API_ADDRESS . '/auth.php');
$client->setOpt(YAR_OPT_PROVIDER, "dummy");
$client->setOpt(YAR_OPT_TOKEN, "dummy");
var_dump($client->normal("dummy"));
?>
--EXPECTF--
string(21) "authentication failed"
string(3) "3.6"
string(3) "3.6"
