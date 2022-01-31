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
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
int(6)

Warning: Yar_Client::setOpt(): expects a string as proxy value in %s033.php on line %d
bool(false)

Warning: Yar_Client::setOpt(): expects a string as proxy value in %s033.php on line %d
bool(false)
Yar_Client Object
(
    [_protocol] => 1
    [_uri] => http://doesnotexists.com
    [_options] => Array
        (
            [6] => 127.0.0.1:8888
        )

    [_running] => 
)

Warning: Yar_Client::setOpt(): proxy only works with HTTP protocol in %s033.php on line %d
bool(false)
Yar_Client Object
(
    [_protocol] => 2
    [_uri] => tcp://127.0.0.1:7690
    [_options] => 
    [_running] => 
)
