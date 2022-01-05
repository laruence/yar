--TEST--
Check for YAR_OPT_TIMEOUT on tcp
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    die("skip");
}
if (getenv("SKIP_SLOW_TESTS")) die("skip slow test");
?>
--INI--
yar.connect_timeout=1000
yar.timeout=1000
yar.packager=php
--FILE--
<?php
include "yar.inc";

yar_tcp_server_start();

$client = new Yar_Client(YAR_TCP_ADDRESS);

var_dump($client->usleep(500000)); /* 500ms */

$client->setOpt(YAR_OPT_TIMEOUT, 100); /* 200ms */

try {
	var_dump($client->usleep(500000));
} catch (Exception $e) {
	var_dump($e->getMessage());
}
?>
--EXPECT--
NULL
string(28) "select timeout 100ms reached"
