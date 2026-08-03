--TEST--
Yar_Client getOpt/setOpt and cross-protocol validation
--CREDITS--
Jarvis (AI assistant to Laruence)
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
// NOTE: getOpt returns false for unset options. setOpt returns $this.
//       header/resolve/proxy only work with HTTP protocol.

$http = new Yar_Client("http://localhost:9999/");

/* getOpt on unset option returns false */
var_dump($http->getOpt(YAR_OPT_PACKAGER));

/* setOpt returns $this for chaining */
var_dump($http->setOpt(YAR_OPT_PACKAGER, "json") === $http);

/* getOpt now returns the set value */
var_dump($http->getOpt(YAR_OPT_PACKAGER));

/* getOpt on a different unset option */
var_dump($http->getOpt(YAR_OPT_TIMEOUT));

/* setOpt then getOpt for timeout (long type) */
$http->setOpt(YAR_OPT_TIMEOUT, 5000);
var_dump($http->getOpt(YAR_OPT_TIMEOUT));

/* setOpt persistent with bool-like value */
$http->setOpt(YAR_OPT_PERSISTENT, true);
var_dump($http->getOpt(YAR_OPT_PERSISTENT));

/* cross-protocol: setOpt header on TCP client triggers warning */
$tcp = new Yar_Client("tcp://localhost:9999/");
$tcp->setOpt(YAR_OPT_HEADER, array("X-Foo: bar"));

/* cross-protocol: setOpt proxy on TCP client triggers warning */
$tcp->setOpt(YAR_OPT_PROXY, "127.0.0.1:8080");

?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECTF--
bool(false)
bool(true)
string(4) "json"
bool(false)
int(5000)
int(1)

Warning: Yar_Client::setOpt(): header only works with HTTP protocol in %s055.php on line %d

Warning: Yar_Client::setOpt(): proxy only works with HTTP protocol in %s055.php on line %d
