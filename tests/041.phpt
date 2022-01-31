--TEST--
Check for timeout of yar concurrent calls
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--INI--
yar.timeout=100
--FILE--
<?php 
include "yar.inc";

yar_server_start();

for ($i = 0; $i < 8; $i++) {
	Yar_Concurrent_Client::call(YAR_API_ADDRESS, "usleep", array(1000000/*500ms*/), NULL, NULL, array(YAR_OPT_TIMEOUT => 1000));
}

Yar_Concurrent_Client::loop(function() {});

--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
Warning: Yar_Concurrent_Client::loop(): %rselect|epoll_wait%r timeout '100ms' reached in %s041.php on line %d
