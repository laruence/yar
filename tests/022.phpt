--TEST--
Check for YAR_OPT_RESOLVE
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    die("skip");
}
if (substr(PHP_OS, 0, 3) == 'WIN') die("skip YAR_OPT_RESOLVE seems doesn't work on Windows");
if (getenv('GITHUB')) die ("skip not sure why not wrok in github integration yet");
if (false == @((new Yar_Client("http://doesnotexists"))->setOpt(YAR_OPT_RESOLVE, array("doesnotexists.com:80:127.0.0.1")))) {
	die("skip require libcurl 7.21.3+");
}
?>
--FILE--
<?php 
include "yar.inc";

yar_server_start();

$client = new Yar_Client("http://doesnotexists.com:" . YAR_API_PORT . "/");
$client->setOpt(YAR_OPT_RESOLVE, array("doesnotexists.com:" . YAR_API_PORT.  ":127.0.0.1"));
var_dump($client->header("hostname"));
var_dump($client->header("user_agent"));
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
string(17) "doesnotexists.com"
string(%d) "PHP Yar RPC-%s"
