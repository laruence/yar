--TEST--
Check for maximum calls of yar concurrent call
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--INI--
report_memleaks=0
--FILE--
<?php 
include "yar.inc";

yar_server_start();

for ($i = 0; $i < 129; $i++) {
	Yar_Concurrent_Client::call(YAR_API_ADDRESS, "usleep", array(1000000/*500ms*/), NULL, NULL, array(YAR_OPT_TIMEOUT => 1000));
}

Yar_Concurrent_Client::reset();

for ($i = 0; $i < 129; $i++) {
	Yar_Concurrent_Client::call(YAR_API_ADDRESS, "usleep", array(1000000/*500ms*/), NULL, NULL, array(YAR_OPT_TIMEOUT => 1000));
}
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
Warning: Yar_Concurrent_Client::call(): too many calls, maximum '128' are allowed in %s042.php on line %d

Warning: Yar_Concurrent_Client::call(): too many calls, maximum '128' are allowed in %s042.php on line %d
