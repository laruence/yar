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
 *   用法：按照正常yar调用方法，只是把类名修改为本调试类名。
 *   1
 *   $yar = new Yar_Debug_Client('http://yar_server/path');
 *   var_dump($yar->call('method', $params));			//这里通常能看到server端代码是否有错误，错误在那里。
 *   2
 *   Yar_Debug_Concurrent_Client::call('http://yar_server/path', 'method', $params, $callback, $errcallback);
 *   Yar_Debug_Concurrent_Client::loop();		//在callback也可以看到server反馈
 *
 *
 *   也可以用这个封装类做一个在线调试 exp:
 *   http://hk.yafdev.com/yar_server_response_viewer.php
 *
 *
***************************************************************************/

class Yar_Debug_Client {
	private $url;
	public function __construct($url) {
		$this->url = $url;
	}

	public function call($method, $arguments) {
		return Yar_Debug_Transports::exec($this->url, Yar_Debug_Protocol::Package($method, $arguments));
	}

	public function __call($method, $arguments) {
		return Yar_Debug_Transports::exec($this->url, Yar_Debug_Protocol::Package($method, $arguments));
	}
}

class Yar_Debug_Concurrent_Client {
	private $data = array();
	public static function call($uri, $m, $params = null, $callback = null, $errorcallback = null) {
		$package = Yar_Debug_Protocol::Package($m, $params);
		$this->data[] = array(
			'uri'		=>	$uri,
			'data'		=>	$package,
			'callback'	=>	$callback,
			'errcb'		=>	$errorcallback,
		);
		return $package['transaction'];
	}

	public static function loop() {
		foreach($this->data as $v) {
			$ret = Yar_Debug_Transports::exec($v['uri'], $v['data']);
			if (strpos('HTTP/1.1 200 OK', $ret['header']) !== false) {
				$call = $v['callback'];
				$return = true;
			} else {
				$call = $v['errcb'];
				$return = false;
			}
			if (is_callable($call)) {
				$o = $ret['o'];
				$r = $ret['r'];
				call_user_func($call, $r, $o);
			}
		}
		return $return;
	}
}

class Yar_Debug_Protocol {
	public static function Package($m, $params) {
		$struct = array(
			'i'	=>	time(),
			'm'	=>	$m,
			'p'	=>	$params,
		);
		$body = str_pad('PHP', 8, chr(0)) . serialize($struct);

		$transaction = sprintf('%08x', mt_rand());

		$header = '';
		$header = $transaction;						//transaction id
		$header .= sprintf('%04x', 0);				//protocl version
		$header .= '80DFEC60';						//magic_num, default is: 0x80DFEC60
		$header .= sprintf('%08x', 0);				//reserved
		$header .= sprintf('%064x', 0);				//reqeust from who
		$header .= sprintf('%064x', 0);				//request token, used for authentication
		$header .= sprintf('%08x', strlen($body));	//request body len

		$data = '';
		for($i = 0; $i < strlen($header); $i = $i + 2)
			$data .= chr(hexdec('0x' . $header[$i] . $header[$i + 1]));
		$data .= $body;
		return array(
			'transaction'	=>	$transaction,
			'data'			=>	$data
		);
	}
}

class Yar_Debug_Transports {
	public static function exec($url, $data) {
		$urlinfo = parse_url($url);
        $port = isset($urlinfo["port"])? $urlinfo["port"] : 80;
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
			die ("cannot conncect to {$address} at port {$port} '{$errstr}'");
		}
		fwrite($fp, $in . $data['data'], strlen($in . $data['data']));

		$f_out = '';
		while ($out = fread($fp, 2048))
			$f_out .= $out;

		$tmp = explode("\r\n\r\n", $f_out);
		return array (
			'header'	=>	$tmp[0],
            'body'      =>  $tmp[1],
			'return'	=>	unserialize(substr($tmp[1], 82 + 8)),
		);
		fclose($fp);
	}
}
?>
