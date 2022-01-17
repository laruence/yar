--TEST--
Check for yar client with output by server
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

var_dump($client->output());

var_dump($client->output_exit());
?>
--EXPECT--
outputstring(7) "success"
outputNULL
