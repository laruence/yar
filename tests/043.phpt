--TEST--
Check for yar server __info
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
		if (strpos($markup, "Service_Provider")) {
			return "okay";
		}
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
    $content = "";
    while (!feof($fp)) {
        $content .= trim(fgets($fp));
    }
    echo substr(rtrim($content), -4, 4);
}
?>
--CLEAN--
<?php
include 'yar.inc';
yar_server_cleanup();
?>
--EXPECT--
okay
