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

function callback($return, $callinfo) {
    global $sequence;

    if ($callinfo) {
        Yar_Concurrent_Client::loop();
        $sequence[$callinfo["sequence"]] = $return;
    }
}

function error_callback($type, $error, $callinfo) {
    global $sequence;
    $sequence[$callinfo["sequence"]] = $error;
}

$sequence[Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array("xxx", "3.8"))] = NULL;

Yar_Concurrent_Client::loop("callback", "error_callback");

ksort($sequence);
print_r($sequence);
--EXPECTF--
Warning: Yar_Concurrent_Client::loop(): concurrent client has already started in %s014.php on line %d
Array
(
    [1] => 3.8
)
