--TEST--
Check for YAR_OPT_PACKAGER on curl
--SKIPIF--
<?php 
if (!extension_loaded("yar") || !extension_loaded("json")) {
    print "skip";
}
die("skip fails on CI"); /* skip for now */
?>
--INI--
yar.debug=1
yar.packager=php
--FILE--
<?php 
include "yar.inc";

yar_server_start();

$client = new Yar_Client(YAR_API_ADDRESS);

$client->setOpt(YAR_OPT_PACKAGER, "json");

$client->normal("dummy");
?>
--EXPECTF--
Warning: [Debug Yar_Client %s]: %d: call api 'normal' at (r)'%s' with '1' parameters in %s040.php on line %d

Warning: [Debug Yar_Client %s]: %d: pack request by 'JSON', result len '%d', content: '{"i":%s,"m":"normal"%s' in %s040.php on line %d

Warning: [Debug Yar_Client %s]: %d: server response content packaged by 'JSON', len '%d', content '{"i":%s,"s":0,"r"%s' in %s040.php on line %d
