--TEST--
Check for yar server __info (return none string)
--SKIPIF--
<?php 
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php 
include "yar.inc";

yar_server_start(<<<'PHP'
<?php
error_reporting(-1);
class Service_Provider {
	protected function __info($markup) {
		return array();
	}
}

$yar = new Yar_Server(new Service_Provider());
$yar->handle();
PHP
);

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
        if (strpos($line, 'Service_Provider - Yar Server') != FALSE) {
            echo "okay";
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
okay
