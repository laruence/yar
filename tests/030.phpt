--TEST--
Check for TCP RPC Exceptions
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
	"s" => 0,
	"e" => "exception",
	"r" => "",
	"o" => "",
);

$exceptions = array(
	"YAR_ERR_PACKAGER",
	"YAR_ERR_PROTOCOL",
	"YAR_ERR_REQUEST",
	"YAR_ERR_OUTPUT",
	"YAR_ERR_TRANSPORT",
	"YAR_ERR_EXCEPTION",
);


$client = new Yar_Client(YAR_TCP_ADDRESS);

foreach ($exceptions as $name) {
	$body['s'] = constant($name);
	$header["body_len"] = strlen(serialize($body)) + 8;
	try {
		$client->raw($header, $body);
	} catch (Exception $e) {
		echo $name , " -> ", get_class($e), "\n";
	}
}
?>
--EXPECT--
YAR_ERR_PACKAGER -> Yar_Client_Packager_Exception
YAR_ERR_PROTOCOL -> Yar_Client_Protocol_Exception
YAR_ERR_REQUEST -> Yar_Client_Exception
YAR_ERR_OUTPUT -> Yar_Client_Exception
YAR_ERR_TRANSPORT -> Yar_Client_Transport_Exception
YAR_ERR_EXCEPTION -> Yar_Server_Exception
