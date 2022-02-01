--TEST--
Check for yar.expose_info off
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--INI--
yar.expose_info=0
--FILE--
<?php 
include "yar.inc";

yar_server_start();

$host = YAR_API_HOSTNAME;
$port = YAR_API_PORT;

if (!$port) {
    $port = 80;
}

$fp = fsockopen($host, $port, $errno, $errstr, 0.5);
if (!$fp) {
  die("connect failed");
}

$uri = YAR_API_URI;

if(fwrite($fp, <<<HEADER
GET /{$uri} HTTP/1.1
Host: {$host}


HEADER
)) {
    while (!feof($fp)) {
        $line = trim(fgets($fp));
        if (strpos($line, 'server info is not allowed to access') != FALSE) {
            echo "okey";
            break;
        }   
    }
}
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECT--
okey
