--TEST--
Check for YAR_OPT_PROVIDER/YAR_OPT_TOKEN
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    die("skip");
}
?>
--FILE--
<?php 
include "yar.inc";

yar_server_start();

$client = new Yar_Client(YAR_API_ADDRESS);
$client->setOpt(YAR_OPT_PROVIDER, array());
var_dump($client->getOpt(YAR_OPT_PROVIDER));
$client->setOpt(YAR_OPT_PROVIDER, str_pad("dummy", 64));
$client->setOpt(YAR_OPT_PROVIDER, str_pad("dummy", 32));

var_dump($client->getOpt(YAR_OPT_PROVIDER));

$client->setOpt(YAR_OPT_TOKEN, 1000);
var_dump($client->getOpt(YAR_OPT_TOKEN));
$client->setOpt(YAR_OPT_TOKEN, str_pad("foo", 64));
$client->setOpt(YAR_OPT_TOKEN, str_pad("foo", 32));

var_dump($client->getOpt(YAR_OPT_TOKEN));
?>
--EXPECTF--
Warning: Yar_Client::setOpt(): expects a maximum 32 bytes string value in %s036.php on line %d
bool(false)

Warning: Yar_Client::setOpt(): expects a maximum 32 bytes string value in %s036.php on line %d
string(32) "dummy                           "

Warning: Yar_Client::setOpt(): expects a maximum 32 bytes string value in %s036.php on line %d
bool(false)

Warning: Yar_Client::setOpt(): expects a maximum 32 bytes string value in %s036.php on line %d
string(32) "foo                             "
