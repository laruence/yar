--TEST--
Check for yar server info with long classname
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
class Test_Info_Page_Service {
    /**
     * say hello 中文字符
     */
    public function hello($name = "world") {
        return "hello " . $name;
    }

    protected function _hidden() {
    }
}

$yar = new Yar_Server(new Test_Info_Page_Service());
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
		if(strpos($line, '<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />') !== false) {
			echo "okey";
			break;
		}
    }
}
?>
--CLEAN--
<?php
include "yar.inc";
yar_server_cleanup();
?>
--EXPECT--
okey
