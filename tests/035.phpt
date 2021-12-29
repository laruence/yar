--TEST--
Check for yar concurrent reset
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
if (substr(PHP_OS, 0, 3) == 'WIN') die("skip doesn't work on Windows");
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
     //   $sequence[0] = NULL;
    }
}

function error_callback($type, $error, $callinfo) {
    global $sequence;
    $sequence[$callinfo["sequence"]] = $error;
}

for ($i=0; $i<3; $i++) {
    $sequence[Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array(1))] = NULL;
}

Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array(1), function($ret, $callinfo) {
	echo "Sequence ", $callinfo["sequence"], " calling reset\n"; 
	var_dump(Yar_Concurrent_Client::reset()); }
);

Yar_Concurrent_Client::loop("callback", "error_callback");

ksort($sequence);
print_r($sequence);

$sequence = array();
for ($i=0; $i<3; $i++) {
    $sequence[Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array(1))] = NULL;
}
Yar_Concurrent_Client::loop("callback");
ksort($sequence);
print_r($sequence);

Yar_Concurrent_Client::reset();
for ($i=0; $i<3; $i++) {
    $sequence[Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array(1))] = NULL;
}
Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array(1), function($ret, $callinfo) {
    global $sequence;
    $sequence[$callinfo["sequence"]] = $ret;
	echo "Sequence ", $callinfo["sequence"], " calling loop\n"; 
	var_dump(Yar_Concurrent_Client::loop("callback"));
}
);
$sequence = array();
Yar_Concurrent_Client::loop(function($ret, $callinfo) {
   if ($callinfo == NULL) {
       echo "Looping start...\n";
   }
   callback($ret, $callinfo);
});

ksort($sequence);
print_r($sequence);
--EXPECTF--
Sequence 4 calling reset

Warning: Yar_Concurrent_Client::reset(): cannot reset while client is running in %s035.php on line %d
bool(false)
Array
(
    [1] => 3.6
    [2] => 3.6
    [3] => 3.6
)
Array
(
    [1] => 3.6
    [2] => 3.6
    [3] => 3.6
)
Looping start...
Sequence 4 calling loop

Warning: Yar_Concurrent_Client::loop(): concurrent client has already been started in %s035.php on line %d
bool(false)
Array
(
    [1] => 3.6
    [2] => 3.6
    [3] => 3.6
    [4] => 3.6
)
