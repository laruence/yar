--TEST--
Check for YAR_OPT_PROXY
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php
$client = new Yar_Client("http://doesnotexists.com");

var_dump(YAR_OPT_PROXY);

var_dump($client->setOpt(YAR_OPT_PROXY, 1));
var_dump($client->setOpt(YAR_OPT_PROXY, []));

print_r($client->setOpt(YAR_OPT_PROXY, "127.0.0.1:8888")); //your http proxy


$tcpClient = new Yar_Client("tcp://127.0.0.1:7690");
var_dump($tcpClient->setOpt(YAR_OPT_PROXY, "127.0.0.1:8888"));
print_r($tcpClient)
?>
--EXPECTF--
int(64)

Warning: Yar_Client::setOpt(): expects a string as proxy value in %s033.php on line %d
bool(false)

Warning: Yar_Client::setOpt(): expects a string as proxy value in %s033.php on line %d
bool(false)
Yar_Client Object
(
    [_protocol:protected] => 1
    [_uri:protected] => http://doesnotexists.com
    [_options:protected] => Array
        (
            [64] => 127.0.0.1:8888
        )

    [_running:protected] => 
)

Warning: Yar_Client::setOpt(): proxy only works with HTTP protocol in %s033.php on line %d
bool(false)
Yar_Client Object
(
    [_protocol:protected] => 2
    [_uri:protected] => tcp://127.0.0.1:7690
    [_options:protected] => 
    [_running:protected] => 
)
