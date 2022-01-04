--TEST--
Check for YAR_OPT_PROVIDER/TOKEN on tcp
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

var_dump($client->info("provider"));

$client->setOpt(YAR_OPT_PROVIDER, "dummy Server");
var_dump($client->info("provider"));

$client->setOpt(YAR_OPT_PROVIDER, str_pad("dummy Server", 64, "x"));
var_dump($client->info("provider"));

var_dump($client->info("token"));

$client->setOpt(YAR_OPT_TOKEN, md5("yar"));
var_dump($client->info("token"));

$client->setOpt(YAR_OPT_TOKEN, str_pad(md5("yar"), 64, "x"));
var_dump($client->info("token"));
?>
--EXPECTF--
string(14) "Yar TCP Client"
string(12) "dummy Server"

Warning: Yar_Client::setOpt(): expects a maximum 32 bytes string value in %s036.php on line %d
string(12) "dummy Server"
string(0) ""
string(32) "883452d07c625e5dbbcdaaa47f0aa92d"

Warning: Yar_Client::setOpt(): expects a maximum 32 bytes string value in %s036.php on line %d
string(32) "883452d07c625e5dbbcdaaa47f0aa92d"
