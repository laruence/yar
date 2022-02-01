--TEST--
Check for yar server __auth (exit)
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
	protected function __auth($provider, $token) {
		exit("nothing");
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
var_dump($client->info());
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECT--
nothingNULL
