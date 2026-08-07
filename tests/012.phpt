--TEST--
Check for yar concurrent client with 64 calls
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
if (getenv("SKIP_SLOW_TESTS")) die("skip slow test");
?>
--FILE--
<?php 
include "yar.inc";

yar_server_start();

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

for ($i=0; $i<64; $i++) {
    $sequence[Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array("xxx", "3.8"))] = NULL;
}

Yar_Concurrent_Client::loop("callback", "error_callback");

ksort($sequence);
print_r($sequence);
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
Array
(
    [0] => 
    [1] => 3.8
    [2] => 3.8
    [3] => 3.8
    [4] => 3.8
    [5] => 3.8
    [6] => 3.8
    [7] => 3.8
    [8] => 3.8
    [9] => 3.8
    [10] => 3.8
    [11] => 3.8
    [12] => 3.8
    [13] => 3.8
    [14] => 3.8
    [15] => 3.8
    [16] => 3.8
    [17] => 3.8
    [18] => 3.8
    [19] => 3.8
    [20] => 3.8
    [21] => 3.8
    [22] => 3.8
    [23] => 3.8
    [24] => 3.8
    [25] => 3.8
    [26] => 3.8
    [27] => 3.8
    [28] => 3.8
    [29] => 3.8
    [30] => 3.8
    [31] => 3.8
    [32] => 3.8
    [33] => 3.8
    [34] => 3.8
    [35] => 3.8
    [36] => 3.8
    [37] => 3.8
    [38] => 3.8
    [39] => 3.8
    [40] => 3.8
    [41] => 3.8
    [42] => 3.8
    [43] => 3.8
    [44] => 3.8
    [45] => 3.8
    [46] => 3.8
    [47] => 3.8
    [48] => 3.8
    [49] => 3.8
    [50] => 3.8
    [51] => 3.8
    [52] => 3.8
    [53] => 3.8
    [54] => 3.8
    [55] => 3.8
    [56] => 3.8
    [57] => 3.8
    [58] => 3.8
    [59] => 3.8
    [60] => 3.8
    [61] => 3.8
    [62] => 3.8
    [63] => 3.8
    [64] => 3.8
)
