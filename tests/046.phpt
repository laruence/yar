--TEST--
Check for yar server __auth (public visiblity)
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php 
include "yar.inc";

yar_server_start(<<<'PHP'
<?php
error_reporting(-1);
class Service_Provider {
	public function __auth($provider, $token) {
        return false;
	}
   
    public function info() {
        return "okay";
    }
}

$yar = new Yar_Server(new Service_Provider());
$yar->handle();
PHP
);

$client = new Yar_Client(YAR_API_ADDRESS);
echo $client->info();
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECT--
okay
