--TEST--
Check for TCP client with server exit
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
	$client->_exit("name");
} catch (Yar_Client_Transport_Exception $e) {
	var_dump($e->getMessage());
}

/* wait for server call its shutdown function */
usleep(500);

try {
	$client->info("id");
} catch (Yar_Client_Transport_Exception $e) {
	var_dump($e->getMessage());
}

--EXPECTF--
string(36) "server closed connection prematurely"
string(%d) "Unable to connect to tcp://%s:%d/ (%s)"
