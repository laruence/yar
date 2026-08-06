--TEST--
Yar server rejects non-array request body unpacked by PHP packager
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

/* A request body which unpacks successfully but is not an array. The
   server side iterates it as a HashTable unconditionally, which used to
   crash on a non-array zval. The JSON/MSGPACK packagers already reject
   non-arrays, only the default PHP packager missed the check. */
$body = "PHP\0YAR_" . serialize("surprise, not an array");

$header = array(
	"id" => 1,
	"version" => 0,
	"magic_num" => YAR_PROTOCOL_MAGIC_NUM,
	"reserved" => 0,
	"provider" => str_pad("attacker", 32),
	"token" => str_pad("", 32),
	"body_len" => strlen($body),
);
$payload = gen_header($header) . $body;

$ctx = stream_context_create(array(
	"http" => array(
		"method" => "POST",
		"header" => "Content-Type: application/octet-stream\r\nContent-Length: " . strlen($payload) . "\r\n",
		"content" => $payload,
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
