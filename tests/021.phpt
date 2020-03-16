--TEST--
Check for YAR_OPT_PERSISTENT
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

$client = new Yar_Client(YAR_API_ADDRESS);
/* warmup */
var_dump($client->header("connection"));
$client->setOpt(YAR_OPT_PERSISTENT, 1);
var_dump($client->header("connection"));
?>
--EXPECTF--
string(5) "close"
string(10) "Keep-Alive"
