--TEST--
Check for TCP RPC Malfromaled response (body_len too small)
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
	"magic_num" => YAR_PROTOCOL_MAGIC_NUM,
	"provider" => str_pad("PHP ILL Server", 32, " "),
	"reserved" => 0,
	"token" => str_pad(" ", 32, " "),
	"version" => 0,
	"body_len" => 0
);

$body = array(
	"i" => 0,
	"s" => 0x40,
	"e" => "exception",
	"r" => "",
	"o" => "",
);

$header["body_len"] = strlen(serialize($body)) + 8 - 8/* less len */;

$client = new Yar_Client(YAR_TCP_ADDRESS);
try {
	$client->raw($header, $body);
} catch (Yar_Client_Exception $e) {
	var_dump($e->getMessage());
}
?>
--EXPECTF--
string(37) "unpack error at offset 77 of 77 bytes"
