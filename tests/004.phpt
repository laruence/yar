--TEST--
Check for yar client
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
--EXPECTF--
Warning: Yar_Client::setOpt(): expects a string packager name in %s004.php on line %d
bool(false)

Warning: Yar_Client::setOpt(): expects a string packager name in %s004.php on line %d
bool(false)
Yar_Client Object
(
    [_protocol:protected] => 1
    [_uri:protected] => http://www.laruence.com/yar/
    [_options:protected] => Array
        (
            [%d] => php
        )

    [_running:protected] => 
)
