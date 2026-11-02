--TEST--
Yar releases curl handles when an HTTP URI cannot be parsed
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
for ($i = 0; $i < 5; $i++) {
	$client = new Yar_Client("http://[");
	try {
		$client->raw_echo("test");
		echo "unexpected\n";
	} catch (Yar_Client_Transport_Exception $e) {
		var_dump(strpos($e->getMessage(), "malformed uri") !== false);
	}
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
