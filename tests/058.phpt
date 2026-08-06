--TEST--
Yar server handles request body shorter than protocol header gracefully
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

/* 86 bytes in total: a valid 82-byte header followed by only 4 bytes of
   body. This used to underflow payload_len (size_t) and feed a huge length
   to the packager, reading out of bounds. */
$header = array(
	"id" => 1,
	"version" => 0,
	"magic_num" => YAR_PROTOCOL_MAGIC_NUM,
	"reserved" => 0,
	"provider" => str_pad("attacker", 32),
	"token" => str_pad("", 32),
	"body_len" => 4,
);
$body = gen_header($header) . "JSON";

$ctx = stream_context_create(array(
	"http" => array(
		"method" => "POST",
		"header" => "Content-Type: application/octet-stream\r\nContent-Length: " . strlen($body) . "\r\n",
		"content" => $body,
		"ignore_errors" => true,
	),
));
/* the server should reject the request but stay alive */
@file_get_contents(YAR_API_ADDRESS, false, $ctx);

$client = new Yar_Client(YAR_API_ADDRESS);
var_dump($client->normal(1, "alive"));
?>
--EXPECTF--
string(5) "alive"
