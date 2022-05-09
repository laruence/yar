--TEST--
ISSUE #172 $provider/$token may not be nullbyte-terminated
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
if (getenv("SKIP_SLOW_TESTS")) die("skip slow test");
?>
--INI--
yar.timeout=10000
--FILE--
<?php 
include "yar.inc";

yar_server_start(<<<'PHP'
<?php
error_reporting(-1);
class Service_Provider {
	protected function __auth($seq, $token) {
          return strlen($token) == intval($seq);
	}
   
    public function info() {
        return "okay";
    }
}

$yar = new Yar_Server(new Service_Provider());
$yar->handle();
PHP
);

$token = str_pad("", 32, chr(127));
for ($seq = 0; $seq < 32; $seq++) {
    $ticket = $token;
    $ticket[$seq] = "\0";
	Yar_Concurrent_Client::call(YAR_API_ADDRESS, "info",
			NULL, NULL, NULL,
			array(
				YAR_OPT_PROVIDER=>"$seq", 
				YAR_OPT_TOKEN=>$ticket,
    ));
}

Yar_Concurrent_Client::loop(function($return, $callinfo) {
}, function($type, $error, $callinfo) {
   echo $error;
      
});
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECT--
