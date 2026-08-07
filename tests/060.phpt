--TEST--
Yar TCP client trusts body_len instead of the bytes actually received
--SKIPIF--
<?php
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--INI--
yar.packager=PHP
--FILE--
<?php
include "yar.inc";

/* TCP only: an HTTP server on the same port would prevent the TCP server
   from binding on non-Windows systems */
yar_tcp_server_start();

/* A malicious server may declare a small body_len while stuffing more
   bytes into the first packet. The client used to memcpy() as many bytes
   as it received into a buffer sized after body_len (heap overflow). */
$header = array(
	"id" => 1,
	"version" => 0,
	"magic_num" => YAR_PROTOCOL_MAGIC_NUM,
	"reserved" => 0,
	"provider" => str_pad("fake server", 32),
	"token" => str_pad("", 32),
	"body_len" => 16,
);
$response = gen_header($header) . str_repeat("A", 500);

$client = new Yar_Client(YAR_TCP_ADDRESS);
try {
	$client->raw_echo($response);
	echo "unexpected\n";
} catch (Yar_Client_Packager_Exception $e) {
	echo "okey\n";
}
?>
--EXPECT--
okey
