--TEST--
Check for yar concurrent client with tcp rpc
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
        var_dump($return);
    }
}

Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array("xxx", "3.8"));
Yar_Concurrent_Client::call("tcp://127.0.0.1:888", "normal", array("xxx", "3.8"));
Yar_Concurrent_Client::call("unix:///tmp/yar.sock", "normal", array("xxx", "3.8"));

Yar_Concurrent_Client::loop("callback");

--EXPECTF--
Warning: Yar_Concurrent_Client::call(): only http protocol is supported in concurrent client for now in %s017.php on line %d

Warning: Yar_Concurrent_Client::call(): only http protocol is supported in concurrent client for now in %s017.php on line %d
string(3) "3.8"
