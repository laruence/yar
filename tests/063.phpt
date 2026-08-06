--TEST--
Yar server must not dispatch non-public methods
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
class Visibility_Service {
	protected function secret() {
		return "leaked";
	}

	private function top_secret() {
		return "leaked";
	}

	public function open() {
		return "public";
	}
}

$yar = new Yar_Server(new Visibility_Service());
$yar->handle();
PHP
);

$client = new Yar_Client(YAR_API_ADDRESS);

var_dump($client->open());

/* protected/private methods used to be dispatchable (zend_call_known_instance_method
   does not check visibility), they must be rejected like undefined APIs */
try {
	var_dump($client->secret());
	echo "failed\n";
} catch (Yar_Client_Exception $e) {
	var_dump(strpos($e->getMessage(), "undefined api") !== false);
}

try {
	var_dump($client->top_secret());
	echo "failed\n";
} catch (Yar_Client_Exception $e) {
	var_dump(strpos($e->getMessage(), "undefined api") !== false);
}
?>
--EXPECT--
string(6) "public"
bool(true)
bool(true)
