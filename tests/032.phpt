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
var_dump($client->strlen(str_pad("dummy", 1000, "y")));
var_dump($client->strlen(str_pad("dummy", 2000, "y")));
var_dump($client->strlen(str_pad("dummy", 3000, "y")));
var_dump($client->strlen(str_pad("dummy", 4000, "y")));
var_dump($client->strlen(str_pad("dummy", 5000, "y")));
var_dump($client->strlen(str_pad("dummy", 6000, "y")));
var_dump($client->strlen(str_pad("dummy", 7000, "y")));
var_dump($client->strlen(str_pad("dummy", 8000, "y")));
var_dump($client->strlen(str_pad("dummy", 9000, "y")));
var_dump($client->strlen(str_pad("dummy", 10000, "y")));
var_dump($client->strlen(str_pad("dummy", 11000, "y")));
?>
--EXPECT--
int(1000)
int(2000)
int(3000)
int(4000)
int(5000)
int(6000)
int(7000)
int(8000)
int(9000)
int(10000)
int(11000)
