--TEST--
Yar client honors YAR_OPT_PROXY
--SKIPIF--
<?php
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php
include "yar.inc";

/* a fake HTTP server acting as the proxy. The target URI points to a dead
   port, so the call can only succeed when the proxy is really used. Before
   the fix YAR_OPT_PROXY passed options[YAR_OPT_CONNECT_TIMEOUT] (NULL) to
   CURLOPT_PROXY, crashing with a NULL dereference. */
$port = yar_find_free_port();
$proxy_server = __DIR__ . DIRECTORY_SEPARATOR . "_yar_proxy_server_" . $port . ".php";
file_put_contents($proxy_server, <<<'PHP'
<?php
$server = stream_socket_server("tcp://127.0.0.1:{$argv[1]}", $errno, $errstr);
if (!$server) {
	exit(1);
}
while ($conn = @stream_socket_accept($server, 30)) {
	$headers = "";
	while (($line = fgets($conn)) !== false) {
		$headers .= $line;
		if (rtrim($line) === "") {
			break;
		}
	}
	if (!preg_match('/Content-Length:\s*(\d+)/i', $headers, $m)) {
		fclose($conn);
		continue;
	}
	$clen = (int)$m[1];
	$body = "";
	while (strlen($body) < $clen && !feof($conn)) {
		$body .= fread($conn, $clen - strlen($body));
	}
	$req_id = strlen($body) >= 4 ? unpack("N", substr($body, 0, 4))[1] : 0;
	$resp_body = "PHP\0YAR_" . serialize(array("i" => $req_id, "s" => 0, "o" => "", "r" => "proxied", "e" => 0));
	$resp_hdr = pack("NnNNA32A32N", $req_id, 0, 0x80DFEC60, 0, str_pad("fake proxy", 32), str_pad("", 32), strlen($resp_body));
	fwrite($conn, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " . strlen($resp_hdr . $resp_body) . "\r\nConnection: close\r\n\r\n" . $resp_hdr . $resp_body);
	fclose($conn);
}
PHP
);

$php_executable = (getenv('TEST_PHP_EXECUTABLE') ?: PHP_BINARY);
$descriptorspec = array(
	0 => array("pipe", "r"),
	1 => array("pipe", "w"),
	2 => array("pipe", "w"),
);
$cmd = "{$php_executable} {$proxy_server} {$port}";
if (substr(PHP_OS, 0, 3) == 'WIN') {
	$handle = proc_open(addslashes($cmd), $descriptorspec, $pipes, NULL, NULL, array("bypass_shell" => true, "suppress_errors" => true));
} else {
	$handle = proc_open($cmd . " 2>/dev/null", $descriptorspec, $pipes);
}

$i = 0;
while (($i++ < 30) && !($fp = @fsockopen("127.0.0.1", $port))) {
	usleep(10000);
}
if ($fp) {
	fclose($fp);
} else {
	die("Cannot start proxy server");
}

register_shutdown_function(function($handle, $proxy_server) {
	proc_terminate($handle);
	@unlink($proxy_server);
}, $handle, $proxy_server);

/* port 1 is closed, only the proxy can answer */
$client = new Yar_Client("http://127.0.0.1:1/", array(YAR_OPT_PROXY => "http://127.0.0.1:{$port}"));
var_dump($client->whatever());
?>
--EXPECT--
string(7) "proxied"
