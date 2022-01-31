--TEST--
Check for yar server __auth
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
		if (!$provider || $provider !== $token) {
			return false;
		} else {
			return true;
		}
	}

	public function normal($uid, $version = "3.6") {
		return $version;
	}
}

$yar = new Yar_Server(new Service_Provider());
$yar->handle();
PHP
);

$client = new Yar_Client(YAR_API_ADDRESS);

try {
    var_dump($client->normal("dummy"));
} catch (Exception $e) {
    var_dump($e->getMessage());
}

$client = new Yar_Client("http://dummy:dummy@" . YAR_API_HOST);
var_dump($client->normal("dummy"));

$client = new Yar_Client(YAR_API_ADDRESS);
$client->setOpt(YAR_OPT_PROVIDER, "dummy");
$client->setOpt(YAR_OPT_TOKEN, "dummy");
var_dump($client->normal("dummy"));
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
string(21) "authentication failed"
string(3) "3.6"
string(3) "3.6"
