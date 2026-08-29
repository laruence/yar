<?php
/*
  +----------------------------------------------------------------------+
  | Yar - Light, concurrent RPC framework                                |
  +----------------------------------------------------------------------+
  | Copyright (c) 2012-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:  Syber       <ibar@163.com>                                  |
  |          Xinchen Hui <laruence@php.net>                              |
  +----------------------------------------------------------------------+
*/

/***************************************************************************
 * A plain-PHP Yar client written against the wire protocol itself, with
 * no dependency on the Yar extension. It speaks the exact binary format
 * documented in the "Protocols" section of README.md:
 *
 *   - an 82-byte header (see yar_header_t in yar_protocol.h): the id,
 *     version, magic number, reserved word, 32-byte provider and token
 *     fields, and the body length; the multi-byte integer fields are
 *     sent in network byte order (big-endian);
 *   - a body made of an 8-byte packager identifier ("PHP", "JSON" or
 *     "MSGPACK", zero-padded) followed by the packed content.
 *
 * Because the raw response is kept available (see Yar_Debug_Transports::exec()),
 * the tool shows exactly what the server replied — which usually makes it
 * easy to locate server-side problems and where they happen.
 *
 * Usage: use it like the regular Yar client, only with the debug class
 * names:
 *
 *   1. synchronous call
 *
 *      $yar = new Yar_Debug_Client('http://yar_server/path');
 *      var_dump($yar->call('method', $params));
 *
 *   2. "concurrent" calls (see the note on Yar_Debug_Concurrent_Client:
 *      requests are registered first and then executed one by one)
 *
 *      Yar_Debug_Concurrent_Client::call('http://yar_server/path', 'method', $params, $callback, $errcallback);
 *      Yar_Debug_Concurrent_Client::loop();
 *
 * A command line wrapper, yar_debug.php, lives next to this file:
 *
 *      php yar_debug.php http://yar_server/path method "arg1,arg2"
 *
 ***************************************************************************/

/** A drop-in replacement for Yar_Client speaking the protocol manually. */
class Yar_Debug_Client {
	private $url;

	public function __construct($url) {
		$this->url = $url;
	}

	/** Calls a remote method explicitly by name. */
	public function call($method, $arguments) {
		return Yar_Debug_Transports::exec($this->url, Yar_Debug_Protocol::Package($method, $arguments));
	}

	/* Any undefined method is turned into an RPC call, like Yar_Client. */
	public function __call($method, $arguments) {
		return Yar_Debug_Transports::exec($this->url, Yar_Debug_Protocol::Package($method, $arguments));
	}
}

/**
 * Registers calls first and executes them in loop(), mimicking
 * Yar_Concurrent_Client's API. Unlike the extension, requests are sent
 * one after another, not truly in parallel; the purpose here is
 * debugging, not performance.
 */
class Yar_Debug_Concurrent_Client {
	private static $data = array();

	/**
	 * Registers a call. Returns the transaction id assigned to it.
	 * No network I/O happens until loop() is called.
	 */
	public static function call($uri, $m, $params = null, $callback = null, $errorcallback = null) {
		$package = Yar_Debug_Protocol::Package($m, $params);
		self::$data[] = array(
			'uri'		=>	$uri,
			'data'		=>	$package,
			'callback'	=>	$callback,
			'errcb'		=>	$errorcallback,
		);
		return $package['transaction'];
	}

	/**
	 * Executes every registered call in registration order and feeds the
	 * decoded response to the callbacks: ('r' => return value,
	 * 'o' => output produced by the service method).
	 */
	public static function loop() {
		$return = true;
		foreach (self::$data as $v) {
			$ret = Yar_Debug_Transports::exec($v['uri'], $v['data']);
			if (strpos($ret['header'], '200') !== false) {
				$call = $v['callback'];
			} else {
				$call = $v['errcb'];
				$return = false;
			}
			if (is_callable($call)) {
				call_user_func($call, $ret['r'], $ret['o']);
			}
		}
		self::$data = array();
		return $return;
	}
}

/** Builds and parses binary Yar messages. */
class Yar_Debug_Protocol {

	/* The Yar protocol header is 82 bytes long (see yar_protocol.h). */
	const HEADER_LEN = 82;

	/* Every message must carry this magic number to be accepted. */
	const MAGIC_NUM = '80DFEC60';

	/*
	 * Packs one request.
	 *
	 * Returns array('transaction' => hex id string, 'data' => the binary
	 * request, ready to be used as an HTTP POST body).
	 */
	public static function Package($m, $params) {
		/* the request body, as decoded on the server side */
		$struct = array(
			'i'	=>	time(),
			'm'	=>	$m,
			'p'	=>	$params,
		);
		/* 8-byte packager identifier, then the packed content */
		$body = str_pad('PHP', 8, chr(0)) . serialize($struct);

		$transaction = sprintf('%08x', mt_rand());

		/*
		 * The header fields, written in the same order as yar_header_t.
		 * sprintf('%0Nx', ...) produces big-endian hex digits, which the
		 * loop below turns into raw bytes — i.e. network byte order.
		 */
		$header = '';
		$header .= $transaction;						// transaction id
		$header .= sprintf('%04x', 0);				// protocol version
		$header .= self::MAGIC_NUM;					// magic_num, default is: 0x80DFEC60
		$header .= sprintf('%08x', 0);				// reserved
		$header .= sprintf('%064x', 0);				// request from who (provider)
		$header .= sprintf('%064x', 0);				// request token, used for authentication
		$header .= sprintf('%08x', strlen($body));	// request body length (including the 8-byte packager identifier)

		/* turn the hex string into raw bytes */
		$data = '';
		for ($i = 0; $i < strlen($header); $i = $i + 2) {
			$data .= chr(hexdec($header[$i] . $header[$i + 1]));
		}
		$data .= $body;
		return array(
			'transaction'	=>	$transaction,
			'data'			=>	$data
		);
	}

	/**
	 * Parses a raw response (header + body) into its fields.
	 *
	 * Returns an array with the header fields ('id', 'version',
	 * 'provider', 'token', 'body_len') plus the decoded body:
	 *   's' => status (one of the YAR_ERR_* codes, 0 on success)
	 *   'r' => return value
	 *   'o' => output captured from the service method
	 *   'e' => error/exception information, when the call failed
	 */
	public static function Unpack($raw) {
		$header = unpack(
			'Nid/nversion/Nmagic/Nreserved/a32provider/a32token/Nbody_len',
			substr($raw, 0, self::HEADER_LEN));

		/* skip the header and the 8-byte packager identifier */
		$body = unserialize(substr($raw, self::HEADER_LEN + 8, $header['body_len'] - 8));

		return $header + (is_array($body) ? $body : array());
	}
}

/** A minimal HTTP transport over plain sockets. */
class Yar_Debug_Transports {

	/**
	 * Sends one request to $url and returns the decoded reply:
	 *   'header'  => the raw HTTP response headers
	 *   'body'    => the raw response body (Yar header + packed content)
	 *   'return'  => the decoded response array (s/r/o/e keys)
	 *   'r' / 'o' => convenience shortcuts for return value and output
	 */
	public static function exec($url, $data) {
		$urlinfo = parse_url($url);
		$port = isset($urlinfo['port']) ? $urlinfo['port'] : (isset($urlinfo['scheme']) && $urlinfo['scheme'] === 'https' ? 443 : 80);
		$path = $urlinfo['path'] . (!empty($urlinfo['query']) ? '?' . $urlinfo['query'] : '') . (!empty($urlinfo['fragment']) ? '#' . $urlinfo['fragment'] : '');

		$in = "POST {$path} HTTP/1.1\r\n";
		$in .= "Host: {$urlinfo['host']}\r\n";
		$in .= "Content-Type: application/octet-stream\r\n";
		$in .= "Connection: Close\r\n";
		$in .= "Hostname: {$urlinfo['host']}\r\n";
		$in .= "Content-Length: " . strlen($data['data']) . "\r\n\r\n";

		$address = gethostbyname($urlinfo['host']);

		$fp = fsockopen($address, $port, $err, $errstr);
		if (!$fp) {
			die ("cannot connect to {$address} at port {$port} '{$errstr}'");
		}
		fwrite($fp, $in . $data['data'], strlen($in . $data['data']));

		$f_out = '';
		while ($out = fread($fp, 2048)) {
			$f_out .= $out;
		}
		fclose($fp);

		$tmp = explode("\r\n\r\n", $f_out, 2);
		$response = Yar_Debug_Protocol::Unpack($tmp[1]);

		return array(
			'header'	=>	$tmp[0],
			'body'		=>	$tmp[1],
			'return'	=>	$response,
			'r'			=>	isset($response['r']) ? $response['r'] : null,
			'o'			=>	isset($response['o']) ? $response['o'] : null,
		);
	}
}
?>
