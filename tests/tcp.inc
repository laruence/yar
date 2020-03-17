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
function parse_header(&$request_body) {
    $header_size = 4 + 2 + 4 + 4 + 32 + 32 + 4;
	$data = substr($request_body, 0, $header_size);
	$header = unpack("Nid/nversion/Nmagic_num/Nreserved/A32provider/A32token/Nbody_len", $data);
	if ($header["magic_num"] != 0x80DFEC60) {
		return array();
	}
	$request_body = substr($request_body, $header_size);
	$packager = substr($request_body, 0, 8);
	$header["packager"] = $packager;
	$request_body = substr($request_body, 8, $header["body_len"]);
	return $header;
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

function server_handle($request_body) {
	$error = NULL;
	$header = parse_header($request_body);
	if (count($header) == 0) {
		return response(NULL, "Malformed request header");	
	}
	if (strncmp($header["packager"], "PHP", 3) != 0) {
		return response(NULL, "Unsupported packager type, only PHP is supported");
	}
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

function start_server($argv, $argc) {
	$host = $argv[1];
	$socket = stream_socket_server($host, $errno, $errstr);
	if (!$socket) {
		echo "$errstr ($errno)\n";
	} else {
		while (1) {
			if ($conn = stream_socket_accept($socket)) {
				$request_body = NULL;
				while (!feof($conn)) {
					$request_body .= fread($conn, 1024);
				}	

				if ($request_body) {
					$response = server_handle($request_body);
					fwrite($conn, $response, strlen($response));
				}
				fclose($conn);
			}
		}
		fclose($socket);
	}
}

start_server($argv, $argc);
