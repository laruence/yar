--TEST--
Check for yar concurrent client with loop in callback
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

$sequence = array();

function error_callback($type, $error, $callinfo) {
    global $sequence;
    $sequence[$callinfo["sequence"]] = $error;
}

$sequence[Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array("xxx", "3.8"))] = NULL;

Yar_Concurrent_Client::loop(function($return, $callinfo) {
    global $sequence;

    if ($callinfo) {
        Yar_Concurrent_Client::loop();
        $sequence[$callinfo["sequence"]] = $return;
    }
}, "error_callback");

ksort($sequence);
print_r($sequence);
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
Warning: Yar_Concurrent_Client::loop(): concurrent client has already been started in %s014.php on line %d
Array
(
    [1] => 3.8
)
