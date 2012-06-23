--TEST--
Check for yar server info
--SKIPIF--
<?php if (!extension_loaded("yar") || version_compare(PHP_VERSION, '5.4.0', '<')) print "skip"; ?>
--FILE--
<?php 
include "yar.inc";
yar_server_start(<<<'DOC'
class Server {

    /**
     * hello world
     * @param string $name
     * @return boolean
     */
    public function helloworld($name) {
       echo "Hello World, " .  $name;
       return TRUE;
    }

    /**
     * default value
     * @param string $provider, default 'yar'
     * @return boolean
     */
    public function foo($provider = "yar") {
       return TRUE;
    }

    /**
     * default value
     * @param string $name
     * @return boolean
     */
    protected function bar($name) {
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


$match = 0;

if(fwrite($fp, <<<HEADER
GET / HTTP/1.1
Host: {$host}


HEADER
)) {
    while (!feof($fp)) {
       $line = trim(fgets($fp));
       if (strpos($line, 'helloworld($name)') != FALSE) {
          $match++;
          continue;
       }
       if (strpos($line, 'foo($provider = \'yar\')') != FALSE) {
          $match++;
          continue;
       }
       if (strpos($line, '@param string $name') != FALSE) {
          $match++;
          continue;
       }
       if (strpos($line, 'bar($name)') != FALSE) {
          $match--;
          continue;
       }
    }
}

var_dump($match);
?>
--EXPECT--
int(3)
