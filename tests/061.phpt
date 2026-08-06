--TEST--
Yar client rejects response body shorter than protocol header
--SKIPIF--
<?php
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php
include "yar.inc";

/* a fake service which answers with a truncated yar response header */
yar_server_start(<<<'PHP'
<?php
$hdr = pack("NnNNA32A32N", 1, 0, 0x80DFEC60, 0, str_pad("PHP Yar Server", 32), str_pad("", 32), 0);
echo substr($hdr, 0, 40);
PHP
);

/* the body (40 bytes) is shorter than yar_header_t (82 bytes); it used to
   underflow payload_len and be reported as an unsupported packager instead
   of a protocol error */
$client = new Yar_Client(YAR_API_ADDRESS);
try {
	$client->normal(1, "1.0");
	echo "unexpected\n";
} catch (Yar_Client_Protocol_Exception $e) {
	echo "okey\n";
}
?>
--EXPECT--
okey
