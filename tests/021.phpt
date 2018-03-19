--TEST--
Check for yar client with custom resolve
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php 
include "yar.inc";

yar_server_start();

$client = new Yar_Client("tcp://127.0.0.1");
$client->setOpt(YAR_OPT_RESOLVE, "string");

$client = new Yar_Client(YAR_API_ADDRESS);
$client->setOpt(YAR_OPT_RESOLVE, "string");
var_dump($client->output());

$port = YAR_API_PORT;

if (!$port) {
    $port = 80;
}

try{
$client = new Yar_Client("http://yar-reslove-test:$port");
$config = array("yar-reslove-test:$port:127.0.0.1");
$client->setOpt(YAR_OPT_RESOLVE, $config);
var_dump($client->output());
}catch(\Exception $e){
	echo $e->getMessage();
}

--EXPECTF--
Warning: Yar_Client::setOpt(): resolve only works with HTTP protocol in %s021.php on line %d

Warning: Yar_Client::setOpt(): expects an array as resolve value in %s021.php on line %d
outputstring(7) "success"
curl exec failed 'Couldn't connect to server'
