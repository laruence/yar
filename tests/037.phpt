--TEST--
Check for YAR_OPT_PERSISTENT on tcp
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php
include "yar.inc";

yar_tcp_server_start();

$client = new Yar_Client(YAR_TCP_ADDRESS);

var_dump($client->info("_conn") == $client->info("_conn"));

$client->setOpt(YAR_OPT_PERSISTENT, true);

var_dump($client->info("_conn") == $client->info("_conn"));
?>
--EXPECTF--
bool(false)
bool(true)
