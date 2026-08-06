--TEST--
Yar client persistent connections are kept alive and reused
--SKIPIF--
<?php
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php
include "yar.inc";

/* a fake keep-alive HTTP server which counts accepted connections. Two
   persistent calls must share one connection. Note: in the CLI harness
   this also passed before the fix (reuse within one request worked with
   the regular_list too); cross-request reuse, which the fix actually
   restores, needs a multi-request SAPI to be observed. This test guards
   the rewritten registration/lookup path. */
$port = 8968;
$count_file = sys_get_temp_dir() . DIRECTORY_SEPARATOR . "yar_persist_" . getmypid() . ".cnt";
$ready_file = sys_get_temp_dir() . DIRECTORY_SEPARATOR . "yar_persist_" . getmypid() . ".rdy";
$server_script = __DIR__ . DIRECTORY_SEPARATOR . "_yar_persist_server.php";
file_put_contents($count_file, "0");
file_put_contents($server_script, <<<'PHP'
<?php
$count_file = $argv[2];
$ready_file = $argv[3];
$server = stream_socket_server("tcp://127.0.0.1:{$argv[1]}", $errno, $errstr);
if (!$server) {
	exit(1);
}
file_put_contents($ready_file, "ready");
$count = 0;
while ($conn = @stream_socket_accept($server, 30)) {
	$count++;
	file_put_contents($count_file, (string)$count);
	/* keep-alive: handle several requests on the same connection */
	while (1) {
		$headers = "";
		$line = fgets($conn);
		if ($line === false) {
			break;
		}
		$headers .= $line;
		while (($line = fgets($conn)) !== false) {
			$headers .= $line;
			if (rtrim($line) === "") {
				break;
			}
		}
		if (!preg_match('/Content-Length:\s*(\d+)/i', $headers, $m)) {
			break;
		}
		$clen = (int)$m[1];
		$body = "";
		while (strlen($body) < $clen && !feof($conn)) {
			$body .= fread($conn, $clen - strlen($body));
		}
		$req_id = strlen($body) >= 4 ? unpack("N", substr($body, 0, 4))[1] : 0;
		$resp_body = "PHP\0YAR_" . serialize(array("i" => $req_id, "s" => 0, "o" => "", "r" => "pong", "e" => 0));
		$resp_hdr = pack("NnNNA32A32N", $req_id, 0, 0x80DFEC60, 0, str_pad("keep-alive", 32), str_pad("", 32), strlen($resp_body));
		$resp = $resp_hdr . $resp_body;
		fwrite($conn, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " . strlen($resp) . "\r\n\r\n" . $resp);
	}
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
$cmd = "{$php_executable} {$server_script} {$port} {$count_file} {$ready_file}";
if (substr(PHP_OS, 0, 3) == 'WIN') {
	$handle = proc_open(addslashes($cmd), $descriptorspec, $pipes, NULL, NULL, array("bypass_shell" => true, "suppress_errors" => true));
} else {
	$handle = proc_open($cmd . " 2>/dev/null", $descriptorspec, $pipes);
}

$i = 0;
while (($i++ < 100) && !file_exists($ready_file)) {
	usleep(10000);
}
if (!file_exists($ready_file)) {
	die("Cannot start server");
}

register_shutdown_function(function($handle, $server_script, $count_file, $ready_file) {
	proc_terminate($handle);
	@unlink($server_script);
	@unlink($count_file);
	@unlink($ready_file);
}, $handle, $server_script, $count_file, $ready_file);

$client = new Yar_Client("http://127.0.0.1:{$port}/", array(YAR_OPT_PERSISTENT => 1));
var_dump($client->ping());
var_dump($client->ping());
var_dump((int)file_get_contents($count_file));
?>
--EXPECT--
string(4) "pong"
string(4) "pong"
int(1)
