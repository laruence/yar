--TEST--
Check for yar server info
--SKIPIF--
<?php if (!extension_loaded("yar")) print "skip"; ?>
--FILE--
<?php 
include "yar.inc";

list($host, $port) = explode(':', YAR_API_HOST);

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
        if (strpos($line, 'Service_Provider - Yar Server') != FALSE) {
            echo "okey";
            break;
        }   
    }
}
?>
--EXPECT--
okey
