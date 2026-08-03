--TEST--
Yar_Concurrent_Client boundary conditions
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
// NOTE: loop() with no calls returns TRUE. reset() with empty list returns TRUE.
//       call() validates URI (must not be empty, must be http/https) and method.

/* loop() with no calls queued returns TRUE immediately */
var_dump(Yar_Concurrent_Client::loop());

/* reset() with empty list returns TRUE */
var_dump(Yar_Concurrent_Client::reset());

/* call() with empty URI triggers warning, returns NULL (not FALSE) */
$r = Yar_Concurrent_Client::call("", "test");
var_dump($r);

/* call() with non-HTTP URI triggers warning (only HTTP in concurrent) */
$r = Yar_Concurrent_Client::call("tcp://localhost/", "test");
var_dump($r);

/* call() with empty method triggers warning */
$r = Yar_Concurrent_Client::call("http://localhost/", "");
var_dump($r);

/* clean up: reset any queued calls from valid attempts */
Yar_Concurrent_Client::reset();

?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
bool(true)
bool(true)

Warning: Yar_Concurrent_Client::call(): first parameter is expected to be a valid rpc server uri in %s057.php on line %d
NULL

Warning: Yar_Concurrent_Client::call(): only http protocol is supported in concurrent client for now in %s057.php on line %d
NULL

Warning: Yar_Concurrent_Client::call(): second parameter is expected to be a valid rpc api name in %s057.php on line %d
NULL
