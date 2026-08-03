--TEST--
Yar_Client unsupported protocol constructor
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
// NOTE: Yar_Client only supports http://, https://, tcp://, unix://.
//       Anything else throws Yar_Client_Protocol_Exception.
//       Yar_Client_Exception::getType() always returns "Yar_Exception_Client".

/* ftp:// is unsupported */
try {
    $client = new Yar_Client("ftp://localhost/");
} catch (Yar_Client_Protocol_Exception $e) {
    echo "Caught: ", get_class($e), "\n";
    var_dump($e->getType());
}

/* bare hostname without protocol is unsupported */
try {
    $client = new Yar_Client("localhost");
} catch (Yar_Client_Protocol_Exception $e) {
    echo "Caught: ", get_class($e), "\n";
}

/* empty string is also unsupported */
try {
    $client = new Yar_Client("");
} catch (Yar_Client_Protocol_Exception $e) {
    echo "Caught: ", get_class($e), "\n";
}

?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
Caught: Yar_Client_Protocol_Exception
string(20) "Yar_Exception_Client"
Caught: Yar_Client_Protocol_Exception
Caught: Yar_Client_Protocol_Exception
