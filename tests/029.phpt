--TEST--
Check for TCP RPC Malfromaled response (incomplete header)
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    die("skip");
}
if (substr(PHP_OS, 0, 3) == 'WIN') die('skip hangs on Windows');
?>
--INI--
yar.packager=php
--FILE--
<?php 
include "yar.inc";

yar_tcp_server_start();

$client = new Yar_Client(YAR_TCP_ADDRESS);
try {
	$client->raw_echo("dummy");
} catch (Yar_Client_Exception $e) {
	var_dump($e->getMessage());
}
?>
--EXPECT--
string(54) "malformed response header, insufficient bytes recieved"
