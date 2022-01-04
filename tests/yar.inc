<?php

define("YAR_API_USER", "dummy");
define("YAR_API_PASS", "foo");

define("YAR_API_HOSTNAME", (getenv('YAR_API_HOSTNAME') ?: "localhost"));
define("YAR_API_PORT",     (getenv('YAR_API_PORT') ?: "8964"));
define("YAR_API_HOST", YAR_API_HOSTNAME . ":" . YAR_API_PORT);
define("YAR_API_URI",  "/");
define("YAR_API_ADDRESS", "http://" . YAR_API_HOST . YAR_API_URI);

function yar_server_start($doc_root = __DIR__ . "/htdocs") {
	$php_executable = (getenv('TEST_PHP_EXECUTABLE')?:PHP_BINARY);
	$cmd_args = NULL;

	if (!(bool)getenv('TRAVIS') && !(bool)getenv('GITHUB')) { /* --show-diff break tests after PHP-7.2 */
		$cmd_args = getenv('TEST_PHP_ARGS');
	}

	if (substr(PHP_OS, 0, 3) == 'WIN') {
		$cmd = "{$php_executable} -t {$doc_root} -n -d extension=php_json.dll {$cmd_args} -S " . YAR_API_HOST;
	} else {
		$cmd_args .= " -d extension=" . dirname(__DIR__) . "/modules/yar.so";
		$cmd = "exec {$php_executable} -t {$doc_root} -n -d extension=json.so {$cmd_args} -S " . YAR_API_HOST . " 2>/dev/null";
	}

	return yar_server_run($cmd);
}

/* For TCP */
define("YAR_TCP_ADDRESS", "tcp://" . YAR_API_HOST . YAR_API_URI);
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
	$php_executable = (getenv('TEST_PHP_EXECUTABLE')?:PHP_BINARY);
	$server = __DIR__ . DIRECTORY_SEPARATOR . "_yar_tcp_server.php";

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

	yar_server_run($cmd);

	register_shutdown_function(
		function($server) {
			unlink($server);
		},
		$server);
}

function yar_server_run($cmd) {
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
    while (($i++ < 30) && !($fp = @fsockopen(YAR_API_HOSTNAME, YAR_API_PORT))) {
        usleep(10000);
    }

    if ($fp) {
        fclose($fp);
	} else {
		die("Cannot start Server with '$cmd'");
	}

	register_shutdown_function(
		function($handle) {
			proc_terminate($handle);
		},
		$handle);
	// don't bother sleeping, server is already up
	// server can take a variable amount of time to be up, so just sleeping a guessed amount of time
	// does not work. this is why tests sometimes pass and sometimes fail. to get a reliable pass
	// sleeping doesn't work.
	return;
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

function response($body, $error) {
	$header = array(
		"id" => rand(100000, 999999),
		"version" => 0,
		"magic_num" => YAR_PROTOCOL_MAGIC_NUM,
		"reserved" => 0,
		"provider" => str_pad("Yar TCP Server", 32),
		"token" => str_repeat(" ", 32),
	);

	$response = array(
		"i" => $header["id"],
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
		return response(NULL, "Unsupported packager type '$packager', only PHP is supported");
	}
	$request_body = substr($request_body, YAR_HEADER_SIZE + 8);
	$request = unserialize($request_body);
	if ($request == false) {
		return response(NULL, "Malformed request body");
	}

	$function = $request["m"];
	$parameters = $request["p"];

	if (!function_exists($function)) {
		return response(NULL, "Unsupported API " . $function);
	}

	set_error_handler(function($no, $msg) use(&$error) { $error = $msg; });

	if ($function == "info") {
		$response = call_user_func_array($function, array($header, $request, $parameters[0]));
	} else if ($function == "raw" || $function == "raw_echo") {
		return call_user_func_array($function, $parameters);
	} else {
		$response = call_user_func_array($function, $parameters);
	}

	return response($response, $error);
}

function verify_header($header) {
	if (count($header) == 0) {
		return response(NULL, "Malformed request header");
	}
	if (!isset($header["magic_num"]) || $header["magic_num"] != YAR_PROTOCOL_MAGIC_NUM) {
		return response(NULL, "Not a legal Yar call");
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
				fwrite($conn, response(NULL, "Unexpected EOL of input, expect $total_byte, $byte_read recived"));
			}
			if ($header["reserved"] & YAR_PROTOCOL_PERSISTENT) {
				goto persistent;
			}
			fclose($conn);
		}
	}
}
?>
