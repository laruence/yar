--TEST--
Check for yar client with output by server
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

var_dump($client->output());

--EXPECT--
outputstring(7) "success"
