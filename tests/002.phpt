--TEST--
Check for yar server info
--SKIPIF--
<?php if (!extension_loaded("yar") || version_compare(PHP_VERSION, '5.4.0', '<')) print "skip"; ?>
--FILE--
<?php 
include "yar.inc";
yar_server_start(<<<'DOC'
class Server {
    /*
     * hello world
     * @param string $name
     * @return boolean
     */
    public function helloworld($name) {
       echo "Hello World, " .  $name;
       return TRUE;
    }
}

$server = new Yar_Server(new Server());
$server->handle();
DOC
, TRUE);

list($host, $port) = explode(':', YAR_SERVER_ADDRESS);

$fp = fsockopen($host, $port, $errno, $errstr, 0.5);
if (!$fp) {
  die("connect failed");
}

if(fwrite($fp, <<<HEADER
GET / HTTP/1.1
Host: {$host}


HEADER
)) {
    while (!feof($fp)) {
     echo fgets($fp);
    }
}
?>
--EXPECT--
xxx
