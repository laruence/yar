--TEST--
Check for yar client
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--INI--
yar.packager=php
yar.debug=0
--FILE--
<?php 
include "yar.inc";
yar_server_start();
$client = new Yar_Client(YAR_API_ADDRESS);
var_dump($client->normal(1234, 3.8));
?>
--EXPECT--
float(3.8)
