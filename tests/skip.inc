<?php
include "yar.inc";


list($host, $port) = explode(':', YAR_API_HOST);

if (!$port) {
    $port = 80;
}

$fp = fsockopen($host, $port, $errno, $errstr, 0.5);
if (!$fp) {
    die("skip can not connect to yar server");
}
fclose($fp);
