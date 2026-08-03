--TEST--
Yar_Server_Exception getType with custom exception class
--CREDITS--
Jarvis (AI assistant to Laruence)
--SKIPIF--
<?php
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php
include "yar.inc";

// NOTE: Yar_Server_Exception::getType() returns the original exception class
//       name from the server, not the wrapper class name. This verifies that
//       custom exception types are preserved across the RPC boundary.

yar_server_start(<<<'PHP'
<?php
error_reporting(-1);
class Service_Provider {
    public function logic_error() {
        throw new LogicException("a logic error occurred");
    }

    public function runtime_error() {
        throw new RuntimeException("a runtime error occurred");
    }

    public function normal_call() {
        return "ok";
    }
}

$yar = new Yar_Server(new Service_Provider());
$yar->handle();
PHP
);

$client = new Yar_Client(YAR_API_ADDRESS);

/* Custom LogicException from server, getType() returns "LogicException" */
try {
    $client->logic_error();
} catch (Yar_Server_Exception $e) {
    var_dump($e->getMessage());
    var_dump($e->getType());
}

/* Custom RuntimeException from server, getType() returns "RuntimeException" */
try {
    $client->runtime_error();
} catch (Yar_Server_Exception $e) {
    var_dump($e->getMessage());
    var_dump($e->getType());
}

/* Sanity: normal call still works */
var_dump($client->normal_call());

?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
string(22) "a logic error occurred"
string(14) "LogicException"
string(24) "a runtime error occurred"
string(16) "RuntimeException"
string(2) "ok"
