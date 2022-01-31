--TEST--
Bug #74867 (segment fault when use yar persistent call twice remote function)
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--INI--
yar.allow_persistent=1
--FILE--
<?php 
include "yar.inc";

yar_server_start();

$host = YAR_API_HOSTNAME;
$port = YAR_API_PORT;

$yar = new Yar_Client('http://' . YAR_API_HOSTNAME . ':' . YAR_API_PORT);
$yar->setOpt(YAR_OPT_PERSISTENT, true);
$yar->output();
$yar->output();
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECT--
outputoutput
