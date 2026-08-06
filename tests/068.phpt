--TEST--
Yar TCP client reports malformed response header without NULL dereference
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
yar_tcp_server_start();

$client = new Yar_Client(YAR_TCP_ADDRESS);

/* a response whose header carries a bad magic number. The error path
   used to format the (still NULL) payload pointer with %s */
try {
	$client->raw_echo(str_repeat("\xff", 100));
	echo "unexpected\n";
} catch (Yar_Client_Protocol_Exception $e) {
	var_dump(strpos($e->getMessage(), "malformed response header") !== false);
	var_dump(strpos($e->getMessage(), "(null)") === false);
}
?>
--EXPECT--
bool(true)
bool(true)
