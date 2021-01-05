--TEST--
Check for tcp protcol
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    die("skip");
}
?>
--INI--
yar.packager=php
--FILE--
<?php 
include "yar.inc";

yar_tcp_server_start();

$client = new Yar_Client(YAR_TCP_ADDRESS);

try {
	$client->dummy("name");
} catch (Yar_Client_Exception $e) {
	var_dump($e->getMessage());
}
/* Use %s below, it maybe negative in 32bits */
var_dump($client->info("id"));
var_dump($client->info("provider"));
try {
	//var_dump($client->pow(4));
} catch (Yar_Client_Exception $e) {
	var_dump($e->getMessage());
}
var_dump($client->pow(4, 2));
--EXPECTF--
string(21) "Unsupported API dummy"
int(%s)
string(14) "Yar TCP Client"
int(16)
