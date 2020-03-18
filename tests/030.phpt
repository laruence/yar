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
	"YAR_ERR_PACKAGER" => 0x1,
	"YAR_ERR_PROTOCOL" => 0x2,
	"YAR_ERR_REQUEST" => 0x4,
	"YAR_ERR_OUTPUT" => 0x8,
	"YAR_ERR_TRANSPORT" => 0x10,
	"YAR_ERR_FORBIDDEN" => 0x20,
	"YAR_ERR_EXCEPTION" => 0x40,
	"YAR_ERR_EMPTY_RESPONSE" => 0x80,
);


$client = new Yar_Client(YAR_TCP_ADDRESS);

foreach ($exceptions as $name => $type) {
	$body['s'] = $type;
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
YAR_ERR_FORBIDDEN -> Yar_Client_Exception
YAR_ERR_EXCEPTION -> Yar_Server_Exception
YAR_ERR_EMPTY_RESPONSE -> Yar_Client_Exception
