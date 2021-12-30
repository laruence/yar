--TEST--
Check for yar concurrent client with add call in callback
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
    static $add = false;

    if ($callinfo) {
        if (!$add) {
            Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array("xxx", "3.8"));
            $add = true;
        }
        
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
Warning: Yar_Concurrent_Client::call(): concurrent client has already been started in %s013.php on line %d
Array
(
    [1] => 3.8
)
