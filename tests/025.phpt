--TEST--
Check for TCP RPC Malfromaled response (Huge body length)
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

$header = array(
	"id" => 0,
	"magic_num" => 0xfff,
	"provider" => str_pad("PHP ILL Server", 32, " "),
	"reserved" => 0,
	"token" => str_pad(" ", 32, " "),
	"version" => 0,
	"body_len" => 0
);

$body = array(
);

$header["body_len"] = strlen(serialize($body)) + 8;

$client = new Yar_Client(YAR_TCP_ADDRESS);
try {
	var_dump($client->raw($header, $body));
} catch (Yar_Client_Protocol_Exception $e) {
	var_dump($e->getMessage());
}

$header["magic_num"] = YAR_PROTOCOL_MAGIC_NUM;
$body = array(
	"i" => 0,
	"s" => 3,
	"e" => "xxx",
	"r" => 1,
	"o" => "",
);
$header["body_len"] = -1;

try {
	var_dump($client->raw($header, $body));
} catch (Yar_Client_Protocol_Exception $e) {
	var_dump($e->getMessage());
}
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
string(34) "malformed response header '(null)'"
string(%d) "response body too large %d"
