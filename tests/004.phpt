--TEST--
Check for Yar_Client::setOpt
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--INI--
yar.packager=php
--FILE--
<?php 
$client = new Yar_Client("http://www.laruence.com/yar/");

var_dump($client->setOpt(YAR_OPT_PACKAGER, 1));
var_dump($client->setOpt(YAR_OPT_PACKAGER, array()));
print_r($client->setOpt(YAR_OPT_PACKAGER, "php"));
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
Warning: Yar_Client::setOpt(): expects a string packager name in %s004.php on line %d
bool(false)

Warning: Yar_Client::setOpt(): expects a string packager name in %s004.php on line %d
bool(false)
Yar_Client Object
(
    [_protocol] => 1
    [_uri] => http://www.laruence.com/yar/
    [_options] => Array
        (
            [%d] => php
        )

    [_running] => 
)
