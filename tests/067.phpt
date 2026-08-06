--TEST--
Yar client handles error responses without a message payload
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

/* an error status without the 'e' field. The client used to format the
   unset err zval as a string (Z_STRVAL on an UNDEF zval) */
try {
	$client->raw_cur(array("s" => 4));
	echo "unexpected\n";
} catch (Yar_Client_Exception $e) {
	var_dump(strpos($e->getMessage(), "unknown error") !== false);
}
?>
--EXPECT--
bool(true)
