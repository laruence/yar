--TEST--
Check for yar concurrent client with custom headers
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php 
include "yar.inc";

yar_server_start();

$client = new Yar_Client("tcp://127.0.0.1");
$client->setOpt(YAR_OPT_HEADER, "string");

$client = new Yar_Client(YAR_API_ADDRESS);
$client->setOpt(YAR_OPT_HEADER, "string");
$client->setOpt(YAR_OPT_HEADER, array("key:val", "foo:bar"));

var_dump($client->header("key"));
var_dump($client->header("foo"));
var_dump($client->header("test"));


--EXPECTF--
Warning: Yar_Client::setOpt(): header only works with HTTP protocol in %s020.php on line %d

Warning: Yar_Client::setOpt(): expects an array as header value in %s020.php on line %d
string(3) "val"
string(3) "bar"
NULL
