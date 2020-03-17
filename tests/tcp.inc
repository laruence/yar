<?php
/*
  typedef struct _yar_header {
    unsigned int   id;            // transaction id
    unsigned short version;       // protocl version
    unsigned int   magic_num;     // default is: 0x80DFEC60
    unsigned int   reserved;
    unsigned char  provider[32];  // reqeust from who
    unsigned char  token[32];     // request token, used for authentication
    unsigned int   body_len;      // request body len
  }
 */
const YAR_HEADER_SIZE = 4 + 2 + 4 + 4 + 32 + 32 + 4;
const YAR_PROTOCOL_PERSISTENT = 0x1;

function parse_header($data) {
	return unpack("Nid/nversion/Nmagic_num/Nreserved/A32provider/A32token/Nbody_len", $data);
}

function gen_header($header) {
	$bin = pack("NnLNA32A32N", 
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
		"magic_num" => 1626136448,
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
	} else {
		$response = call_user_func_array($function, $parameters);
	}

	return response($response, $error);
}

function verify_header($header) {
	if (count($header) == 0) {
		return response(NULL, "Malformed request header");	
	}
	if (!isset($header["magic_num"]) || $header["magic_num"] != 0x80DFEC60) {
		return response(NULL, "Not a legal Yar call");	
	}

	return true;
}

function start_server($argv, $argc) {
	$host = $argv[1];
	$socket = stream_socket_server($host, $errno, $errstr);
	if (!$socket) {
		echo "$errstr ($errno)\n";
	} else {
		while (1) {
			if ($conn = stream_socket_accept($socket)) {
persistent:
				$buf = fread($conn, YAR_HEADER_SIZE);
				if (empty($buf)) {
					fclose($conn);
					continue;
				}
				$header = parse_header($buf);
				if (($status = verify_header($header)) !== true) {
					fwrite($conn, $status, strlen($stats));
					fclose($conn);
					continue;
				}
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
		fclose($socket);
	}
}

start_server($argv, $argc);
