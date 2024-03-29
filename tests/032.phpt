--TEST--
Check for yar client with huge request body
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
var_dump($client->strlen(str_pad("dummy", 500, "y")));
var_dump($client->strlen(str_pad("dummy", 1000, "y")));
var_dump($client->strlen(str_pad("dummy", 10000, "y")));
var_dump($client->strlen(str_pad("dummy", 100000, "y")));
var_dump($client->strlen(str_pad("dummy", 1000000, "y")));
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECT--
int(500)
int(1000)
int(10000)
int(100000)
int(1000000)
