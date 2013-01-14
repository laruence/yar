--TEST--
Check for yar concurrent client with throw exception in callback
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
include "skip.inc";
?>
--FILE--
<?php 
include "yar.inc";

function callback($return, $callinfo) {
    global $sequence;

    if ($callinfo) {
        throw new Exception("exception in callback");
    }
}

Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array("xxx", "3.8"));
Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array("xxx", "3.8"));
Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array("xxx", "3.8"));

try {
    Yar_Concurrent_Client::loop("callback");
} catch (Exception $e) {
    var_dump($e->getMessage());
}

--EXPECTF--
string(21) "exception in callback"
