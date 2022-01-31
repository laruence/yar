--TEST--
Check for yar client with 302 response
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
header("Location: http://www.laruence.com");
PHP
);

$client = new Yar_Client(YAR_API_ADDRESS);

try {
    $client->fatal();
} catch (Yar_Client_Transport_Exception $exceptin) {
    var_dump($exceptin->getMessage());
}
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECT--
string(35) "server responsed non-200 code '302'"
