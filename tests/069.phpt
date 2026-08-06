--TEST--
Yar client rejects responses whose id does not match the request
--SKIPIF--
<?php
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php
include "yar.inc";

/* TCP only: an HTTP server on the same port would prevent the TCP server
   from binding on non-Windows systems */
yar_tcp_server_start();

/* a response carrying a transaction id different from the (random) request
   id, e.g. from another client's request on a mixed-up proxy */
$header = array(
	"id" => 999999,
	"magic_num" => YAR_PROTOCOL_MAGIC_NUM,
	"provider" => str_pad("evil server", 32),
	"reserved" => 0,
	"token" => str_pad("", 32),
	"version" => 0,
	"body_len" => 0,
);
$body = array(
	"i" => 999999,
	"s" => 0,
	"o" => "",
	"r" => "hijacked",
	"e" => 0,
);
$header["body_len"] = strlen("PHP\0YAR_" . serialize($body));

$client = new Yar_Client(YAR_TCP_ADDRESS);
try {
	var_dump($client->raw($header, $body));
	echo "unexpected\n";
} catch (Yar_Client_Protocol_Exception $e) {
	var_dump(strpos($e->getMessage(), "id mismatch") !== false);
}
?>
--EXPECT--
bool(true)
