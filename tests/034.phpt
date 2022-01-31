--TEST--
Check for Yar_Client::__cosntruct with options
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
$client = new Yar_Client("http://www.laruence.com/yar/", array(
     YAR_OPT_PACKAGER => "php",
     YAR_OPT_PROXY => "127.0.0.1:9000",
     YAR_OPT_PERSISTENT => 1,
     YAR_OPT_HEADER => array("n:v"),
));
print_r($client);

try {
	$client = new Yar_Client("http://www.laruence.com/yar/", array(
		 YAR_OPT_PACKAGER => 1,
	));
} catch (Exception $e) {
   print $e->getMessage();
}
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
Yar_Client Object
(
    [_protocol] => 1
    [_uri] => http://www.laruence.com/yar/
    [_options] => Array
        (
            [0] => php
            [1] => 1
            [4] => Array
                (
                    [0] => n:v
                )

            [6] => 127.0.0.1:9000
        )

    [_running] => 
)

Warning: Yar_Client::__construct(): expects a string packager name in %s034.php on line %d
illegal option
