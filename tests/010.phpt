--TEST--
Check for yar debug
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
if (substr(PHP_OS, 0, 3) == 'WIN') die('skip Fails on 32bits Windows');
?>
--INI--
yar.debug=1
yar.packager=php
--FILE--
<?php 
include "yar.inc";

yar_server_start();

$client = new Yar_Client(YAR_API_ADDRESS);

$client->normal("dummy");
?>
--EXPECTF--
Warning: [Debug Yar_Client %s]: %d: call api 'normal' at (r)'%s' with '1' parameters in %s010.php on line %d

Warning: [Debug Yar_Client %s]: %d: pack request by 'PHP', result len '%d', content: 'a:3:{s:1:"i";i:%s' in %s010.php on line %d

Warning: [Debug Yar_Client %s]: %d: server response content packaged by 'PHP', len '%d', content 'a:3:{s:1:"i";i%s' in %s010.php on line %d
