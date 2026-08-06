--TEST--
Yar client setOpt/getOpt bounds and option replacement
--SKIPIF--
<?php
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php
/* no server needed, options are handled client-side */
$client = new Yar_Client("http://127.0.0.1:8964/");

/* setting the same option twice must replace (and release) the old value */
$client->setOpt(YAR_OPT_PACKAGER, "php");
$client->setOpt(YAR_OPT_PACKAGER, "json");
var_dump($client->getOpt(YAR_OPT_PACKAGER));

/* out-of-range option ids used to index the options array unchecked */
var_dump($client->setOpt(9999, "foo"));
var_dump($client->getOpt(9999));
var_dump($client->getOpt(-1));
?>
--EXPECT--
string(4) "json"
bool(false)
bool(false)
bool(false)
