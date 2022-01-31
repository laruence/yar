--TEST--
Check for yar concurrent client with exit in callbacks
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

function callback($return, $callinfo) {
    static $count = 0;
	if (++$count == 4) {
		exit("exit");
	}
}

class Invoke {
   public function __invoke($return, $callinfo) {
       callback($return, $callinfo);
   }
}

class Callback {
    public static function exec($return, $callinfo) {
         callback($return, $callinfo);
    }
}

Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array("xxx", "3.8"), array("Callback", "exec"));
Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array(str_pad("x", 1024, "x"), "3.8"), new Invoke());
Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array(str_pad("x", 10240, "x"), "3.8"), array(new Invoke(), "__invoke"));
Yar_Concurrent_Client::call(YAR_API_ADDRESS, "normal", array(str_pad("x", 102400, "x"), "3.8"));

try {
    Yar_Concurrent_Client::loop("callback", function() { exit("error"); });
} catch (Exception $e) {
    var_dump($e->getMessage());
}

--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
exit
