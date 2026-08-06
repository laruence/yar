--TEST--
Yar ssl_verify INI switch
--SKIPIF--
<?php
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php
/* smoke test for the INI plumbing only; actual TLS verification can not be
   exercised in the test harness */
var_dump(ini_get("yar.ssl_verify"));
ini_set("yar.ssl_verify", 1);
var_dump(ini_get("yar.ssl_verify"));
?>
--EXPECT--
string(0) ""
string(1) "1"
