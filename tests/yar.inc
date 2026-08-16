<?php

define("YAR_API_USER", "dummy");
define("YAR_API_PASS", "foo");

define("YAR_API_HOSTNAME", (getenv('YAR_API_HOSTNAME') ?: "127.0.0.1")); // avoid "localhost" which binds IPv6-only on macOS, breaking YAR_OPT_RESOLVE tests
define("YAR_API_URI",  "/");

/* yar_server_start()/yar_tcp_server_start() allocate the server port
   dynamically (port 0) and return it; the YAR_API_PORT/YAR_API_HOST/
   YAR_API_ADDRESS (and YAR_TCP_ADDRESS) constants are defined at that
   point.  The port doubles as a unique id: it names the built-in server
   doc root (htdocs_<port>/) and the generated tcp server script, so
   tests may run in parallel (PHP 8.6's run-tests.php runs up to 10
   workers by default).  Set YAR_API_PORT in the environment to pin a
   fixed port instead. */

function yar_server_dir($port) {
	return dirname(__FILE__) . DIRECTORY_SEPARATOR . "htdocs_" . $port;
}

/* ask the kernel for an ephemeral port instead of the static 8964, which
   collided when tests ran in parallel */
function yar_pick_port() {
	$sock = @stream_socket_server("tcp://" . YAR_API_HOSTNAME . ":0", $errno, $errstr);
	if (!$sock) {
		die("Cannot allocate a free port: $errstr ($errno)");
	}
	$name = stream_socket_get_name($sock, false);
	$port = (int)substr($name, strrpos($name, ":") + 1);
	fclose($sock);
	return $port;
}

function yar_rmdir($dir) {
	if (is_dir($dir)) {
		$dp = opendir($dir);
		while (($f = readdir($dp))) {
			if (in_array($f, array('.', '..'))) {
				continue;
			}
			$path = $dir . DIRECTORY_SEPARATOR . $f;
			if (is_file($path)) {
				@unlink($path);
			}
		}
		closedir($dp);
		/* on Windows the killed server may still hold the directory for a
		   moment after proc_terminate(); retry briefly, and keep the
		   cleanup quiet either way so it does not leak into test output */
		for ($i = 0; $i < 10; $i++) {
			if (@rmdir($dir)) {
				return;
			}
			usleep(50000);
		}
	}
}

function yar_server_start($code = NULL) {
	$port = yar_setup_test_port();
	$doc_root = yar_server_dir($port);
	$php_executable = (getenv('TEST_PHP_EXECUTABLE')?:PHP_BINARY);
	$cmd_args = NULL;

	yar_server_build($doc_root, $code);
	if (!(bool)getenv('TRAVIS') && !(bool)getenv('GITHUB')) { /* --show-diff break tests after PHP-7.2 */
		$cmd_args = getenv('TEST_PHP_ARGS');
	}

	$cmd_args .= " -d yar.expose_info=" . ini_get("yar.expose_info");
	$cmd_args .= " -d yar.content_type=" . ini_get("yar.content_type");
	$cmd_args .= " -d yar.allow_persistent=" . ini_get("yar.allow_persistent");

	if (substr(PHP_OS, 0, 3) == 'WIN') {
		$cmd_args = " -d extension=php_json.dll " . $cmd_args;
		if (YAR_HAS_MSGPACK) {
			$cmd_args = " -d extension=php_msgpack.dll " . $cmd_args;
		}
		$cmd_args .= " -d extension=php_yar.dll";
		$cmd = "{$php_executable} -t {$doc_root} -n {$cmd_args} -S " . YAR_API_HOST;
	} else {
		if (PHP_VERSION_ID < 80000) {
			$cmd_args = " -d extension=json.so " . $cmd_args;
		}
		if (YAR_HAS_MSGPACK) {
			$cmd_args = " -d extension=msgpack.so " . $cmd_args;
		}
		$cmd_args .= " -d extension=" . dirname(__DIR__) . "/modules/yar.so";
		$cmd = "exec {$php_executable} -t {$doc_root} -n {$cmd_args} -S " . YAR_API_HOST . " 2>/dev/null";
	}

	$handle = yar_server_run($cmd, $port);

	register_shutdown_function(function($handle, $doc_root) {
		proc_terminate($handle);
		yar_rmdir($doc_root);
	}, $handle, $doc_root);

	$GLOBALS['yar_http_server_started'] = true;

	return $port;
}

function yar_server_build($dir, $content = NULL) {
	if (!file_exists($dir)) {
		mkdir($dir, 0755, true);
		chmod($dir, 0755);
	}

	if (!$content) {
		$content = <<<'PHP'
<?php
class Service_Provider {
	/**
	 * @param interge $uid
	 * @param string $version
	 * @return string
	 */
	public function normal($uid, $version = "3.6") {
		return $version;
	}

	public function exception() {
		throw new Exception("server exceptin");
	}

	public function output() {
		echo "output";
		return "success";
	}

	public function output_exit() {
		echo "output";
		exit(1);
	}

	public function strlen($str) {
		return strlen($str);
	}

	public function header($name) {
		$key = "HTTP_" . strtoupper($name);
		return isset($_SERVER[$key])? $_SERVER[$key] : NULL;
	}

	public function usleep($usec) {
		usleep($usec);
		return 1;
	}

	protected function invisible() {
	}
}

$yar = new Yar_Server(new Service_Provider());
$yar->handle();
PHP;
	}

	$file = $dir . DIRECTORY_SEPARATOR . "index.php";
	file_put_contents($file, $content);
	return true;
}

/* shared by yar_server_start() and yar_tcp_server_start(): pick (or honor
   the env override for) the port, define the YAR_API_* constants the
   tests reference, and claim a htdocs_<port>/ doc root */
function yar_setup_test_port() {
	static $port = NULL;
	if ($port !== NULL) {
		return $port;
	}

	$port = getenv('YAR_API_PORT');
	if (!$port) {
		$doc_root = NULL;
		/* retry a few times: the probed port may have been grabbed between
		   the probe and the doc root creation */
		for ($i = 0; $i < 10; $i++) {
			$port = yar_pick_port();
			$doc_root = yar_server_dir($port);
			if (@mkdir($doc_root, 0755)) {
				break;
			}
			if (!file_exists($doc_root)) {
				die("Cannot create test docroot '$doc_root'");
			}
			$doc_root = NULL;
		}
		if ($doc_root === NULL) {
			die("Cannot find a free port for the yar test server");
		}
	} else {
		$port = (int)$port;
	}

	if (!defined('YAR_API_PORT')) {
		define("YAR_API_PORT", $port);
		define("YAR_API_HOST", YAR_API_HOSTNAME . ":" . YAR_API_PORT);
		define("YAR_API_ADDRESS", "http://" . YAR_API_HOST . YAR_API_URI);
	}

	return $port;
}

/* backwards compatible with the --CLEAN-- sections of the tests: doc
   roots are now removed by the shutdown handler registered in
   yar_server_start(), so nothing is left for a separate process to do */
function yar_server_cleanup() {
}

/* For TCP */
/*
  typedef struct _yar_header {
    uint32_t       id;            // transaction id
    uint16_t       version;       // protocl version
    uint32_t       magic_num;     // default is: 0x80DFEC60
    uint32_t       reserved;
    unsigned char  provider[32];  // reqeust from who
    unsigned char  token[32];     // request token, used for authentication
    uint32_t       body_len;      // request body len
  }__attribute__ ((packed))
 */
define("YAR_HEADER_SIZE", 4 + 2 + 4 + 4 + 32 + 32 + 4);
if (PHP_INT_SIZE == 8) {
	define("YAR_PROTOCOL_MAGIC_NUM", 0x80DFEC60);
} else {
	define("YAR_PROTOCOL_MAGIC_NUM", unpack("N", pack("H*", "80DFEC60"))[1]);
}
define("YAR_PROTOCOL_PERSISTENT", 0x1);

function yar_tcp_server_start() {
	$port = yar_setup_test_port();
	if (!defined('YAR_TCP_ADDRESS')) {
		define("YAR_TCP_ADDRESS", "tcp://" . YAR_API_HOST . YAR_API_URI);
	}

	$php_executable = (getenv('TEST_PHP_EXECUTABLE')?:PHP_BINARY);
	$server = __DIR__ . DIRECTORY_SEPARATOR . "_yar_tcp_server_" . $port . ".php";

	file_put_contents($server, <<<'PHP'
<?php
   require("yar.inc");
   yar_tcp_server_handle($argv, $argc);
?>
PHP
);
	if (!file_exists($server)) {
		die("Can not write server script");
	}

	if (substr(PHP_OS, 0, 3) == 'WIN') {
		$cmd = "{$php_executable} {$server} " . YAR_TCP_ADDRESS;
	} else {
		$cmd = "exec {$php_executable} {$server} " . YAR_TCP_ADDRESS . " 2>/dev/null";
	}

	$handle = yar_server_run($cmd, $port);

	register_shutdown_function(
		function($handle, $server, $port) {
			proc_terminate($handle);
			unlink($server);
			/* the doc root yar_setup_test_port() created is only used by the
			   built-in http server; remove it if this test didn't start one */
			if (empty($GLOBALS['yar_http_server_started'])) {
				yar_rmdir(yar_server_dir($port));
			}
		},
		$handle, $server, $port);

	return $port;
}

function yar_server_run($cmd, $port) {
	$descriptorspec = array(
		0 => array("pipe", "r"),
		1 => array("pipe", "w"),
		2 => array("pipe", "w"),
	);
	if (substr(PHP_OS, 0, 3) == 'WIN') {
		$handle = proc_open(addslashes($cmd), $descriptorspec, $pipes, NULL, NULL, array("bypass_shell" => true,  "suppress_errors" => true));
	} else {
		$handle = proc_open($cmd, $descriptorspec, $pipes, NULL);
	}

	// note: even when server prints 'Listening on localhost:8964...Press Ctrl-C to quit.'
	//       it might not be listening yet...need to wait until fsockopen() call returns
    $i = 0;
    while (($i++ < 30) && !($fp = @fsockopen(YAR_API_HOSTNAME, $port))) {
        usleep(10000);
    }

    if ($fp) {
        fclose($fp);
	} else {
		die("Cannot start Server with '$cmd'");
	}

	// don't bother sleeping, server is already up
	// server can take a variable amount of time to be up, so just sleeping a guessed amount of time
	// does not work. this is why tests sometimes pass and sometimes fail. to get a reliable pass
	// sleeping doesn't work.
	return $handle;
}

function parse_header($data) {
	return unpack("Nid/nversion/Nmagic_num/Nreserved/A32provider/A32token/Nbody_len", $data);
}

function gen_header($header) {
	$bin = pack("NnNNA32A32N",
		$header["id"],
		$header["version"],
		$header["magic_num"],
		$header["reserved"],
		$header["provider"],
		$header["token"],
		$header["body_len"]
	);
	return $bin;
}

function response($req_id, $body, $error) {
	$header = array(
		"id" => rand(100000, 999999),
		"version" => 0,
		"magic_num" => YAR_PROTOCOL_MAGIC_NUM,
		"reserved" => 0,
		"provider" => str_pad("Yar TCP Server", 32),
		"token" => str_repeat(" ", 32),
	);

	$response = array(
		/* echo the request id so clients may match it; error paths, which
		   failed to parse the request, use 0 */
		"i" => $req_id,
		"s" => 0,
		"o" => "",
		"r" => NULL,
		"e" => 0,
	);

	if ($error == NULL) {
		$response["r"] = $body;
	} else {
		$response["e"] = $error;
		$response["s"] = 4;
	}

	$res_str = serialize($response);
	$res_str = "PHP\0YAR_" . $res_str;
	$header["body_len"] = strlen($res_str);
	$header_str = gen_header($header);
	$res_str = $header_str .  $res_str;

	return $res_str;
}

function raw_echo($str) {
	return $str;
}

function raw($header, $response) {
	$res_str = serialize($response);
	$header_str = gen_header($header);

	return $header_str . "PHP\0YAR_" . $res_str;
}

/* same as raw(), but echoes the id of the current request, so the client
   accepts the response (needed for testing client-side error handling) */
function raw_cur($header, $params) {
	$response = $params[0];
	$response["i"] = $header["id"];

	$res_str = "PHP\0YAR_" . serialize($response);
	$header["body_len"] = strlen($res_str);

	return gen_header($header) . $res_str;
}

function _exit() {
	exit();
}

function info($header, $request, $name) {
	return $header[$name];
}

function server_handle($header, $request_body) {
	$error = NULL;
	$packager = substr($request_body, YAR_HEADER_SIZE, 3);
	if (strncmp($packager, "PHP", 3) != 0) {
		return response(0, NULL, "Unsupported packager type '$packager', only PHP is supported");
	}
	$request_body = substr($request_body, YAR_HEADER_SIZE + 8);
	$request = unserialize($request_body);
	if ($request == false) {
		return response(0, NULL, "Malformed request body");
	}

	$function = $request["m"];
	$parameters = $request["p"];

	if (!function_exists($function)) {
		return response(0, NULL, "Unsupported API " . $function);
	}

	set_error_handler(function($no, $msg) use(&$error) { $error = $msg; });

	if ($function == "info") {
		$response = call_user_func_array($function, array($header, $request, $parameters[0]));
	} else if ($function == "raw" || $function == "raw_echo") {
		return call_user_func_array($function, $parameters);
	} else if ($function == "raw_cur") {
		return call_user_func_array($function, array($header, $parameters));
	} else {
		$response = call_user_func_array($function, $parameters);
	}

	return response($header["id"], $response, $error);
}

function verify_header($header) {
	if (count($header) == 0) {
		return response(0, NULL, "Malformed request header");
	}
	if (!isset($header["magic_num"]) || $header["magic_num"] != YAR_PROTOCOL_MAGIC_NUM) {
		return response(0, NULL, "Not a legal Yar call");
	}

	return true;
}

function yar_tcp_server_handle($argv, $argc) {
	$host = $argv[1];
	$socket = stream_socket_server($host, $errno, $errstr);
	if (!$socket) {
		echo "$errstr ($errno)\n";
	} else {
		register_shutdown_function(function($socket) {
			fclose($socket);
		}, $socket);

		while ($conn = stream_socket_accept($socket, 5)) {
			/* in case of unexpected exit while handling */
			register_shutdown_function(function($conn) {
				fclose($conn);
			}, $conn);
persistent:
			$buf = fread($conn, YAR_HEADER_SIZE);
			if (empty($buf)) {
				fclose($conn);
				continue;
			}
			$header = parse_header($buf);
			if (($status = verify_header($header)) !== true) {
				fwrite($conn, $status, strlen($status));
				fclose($conn);
				continue;
			}
			/* _conn is only used for testing (check persistent) */
			$header["_conn"] = (int)$conn;
			$content_len = $header["body_len"];
			$total_byte = $content_len + YAR_HEADER_SIZE;
			$byte_read = YAR_HEADER_SIZE;
			while (($byte_read != $total_byte && !feof($conn))) {
				$buf .= fread($conn, $total_byte - $byte_read);
				$byte_read = strlen($buf);
			}
			if ($byte_read == $total_byte) {
				$response = server_handle($header, $buf);
				fwrite($conn, $response, strlen($response));
			} else {
				fwrite($conn, response(0, NULL, "Unexpected EOL of input, expect $total_byte, $byte_read recived"));
			}
			if ($header["reserved"] & YAR_PROTOCOL_PERSISTENT) {
				goto persistent;
			}
			fclose($conn);
		}
	}
}
?>
