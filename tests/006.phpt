--TEST--
Check for yar client with exception
--SKIPIF--
<?php if (!extension_loaded("yar")) print "skip"; ?>
--FILE--
<?php 
include "yar.inc";

$sequence = array();

function callback($return, $callinfo) {
    global $sequence;
    if ($callinfo) {
        $sequence[$callinfo["sequence"]] = $return;
    } else {
        $sequence[0] = NULL;
    }
}

function error_callback($type, $error, $callinfo) {
    global $sequence;
    $sequence[$callinfo["sequence"]] = $error;
}

$sequence[Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array("xxx", "3.8"))] = NULL;
$sequence[Yar_Concurrent_Client::call(YAR_API_ADDRESS, "exception", array(), "callback")] = NULL;
Yar_Concurrent_Client::loop("callback", "error_callback");

ksort($sequence);
print_r($sequence);
--EXPECTF--
Array
(
    [0] => 
    [1] => 3.8
    [2] => Array
        (
            [message] => server exceptin
            [code] => 0
            [file] => %s
            [line] => %d
            [_type] => Exception
        )

)
