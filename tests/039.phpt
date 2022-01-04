--TEST--
Check for debug on TCP
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    die("skip");
}
if (getenv("SKIP_SLOW_TESTS")) die("skip slow test");
?>
--INI--
yar.connect_timeout=1000
yar.timeout=1000
yar.debug=1
--FILE--
<?php
include "yar.inc";

yar_tcp_server_start();

$client = new Yar_Client(YAR_TCP_ADDRESS);
$client->info("_conn");
?>
--EXPECTF--
Warning: [Debug Yar_Client %s]: %d: call api 'info' at (r)'%s' with '1' parameters in %s039.php on line %d

Warning: [Debug Yar_Client %s]: %d: pack request by 'PHP', result len '%d', content: 'a:3:{s:1:"i";i:%s' in %s039.php on line %d

Warning: [Debug Yar_Client %s]: %d: server response content packaged by 'PHP', len '%d', content 'a:5:{s:1:"i";i%s' in %s039.php on line %d
