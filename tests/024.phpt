--TEST--
Check for setopt on tcp
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

var_dump($client->info("id"));

$client->setOpt(YAR_OPT_PACKAGER, "json");
try {
	var_dump($client->info("id"));
} catch (Exception $e) {
	var_dump($e->getMessage());
}

$client->setOpt(YAR_OPT_PACKAGER, "php");
$client->setOpt(YAR_OPT_RESOLVE, "localhost:80:127.0.0.1");

var_dump($client->info("reserved"));
$client->setOpt(YAR_OPT_PERSISTENT, 1);
var_dump($client->info("reserved"));
var_dump($client->info("provider"));
?>
--EXPECTF--
int(%d)
string(54) "Unsupported packager type 'JSO', only PHP is supported"

Warning: Yar_Client::setOpt(): resolve only works with HTTP protocol in %s024.php on line %d
int(0)
int(1)
string(14) "Yar TCP Client"
