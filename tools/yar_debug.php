<?php
/***************************************************************************
 *                                 yar_debug.php
 *                            -------------------------
 *   begin                : Thursday, Apr 27, 2006
 *   copyright            : (C) 2002 The Bleso.com
 *   email                : 1881191@qq.com
 *   qq                   : 1881191
 *   msn                  : ibar@163.com
 *   last modify on       : 
 *
 *   $Id: yar_debug.php, v 0.0.0.1 2013/12/7 23:52:07 Syber Exp $
 *
 *   用法：按照正常yar调用方法，只是把类名修改为本调试类名。
 *   1
 *   $yar = new dYar_client('http://yar_server/path');
 *   var_dump($yar->call('method', $params));			//这里通常能看到server端代码是否有错误，错误在那里。
 *   2
 *   dYar_Concurrent_Client::call('http://yar_server/path', 'method', $params, $callback, $errcallback);
 *   dYar_Concurrent_Client::loop();		//在callback也可以看到server反馈
 *
 *
 *   也可以用这个封装类做一个在线调试 exp:
 *   http://hk.yafdev.com/yar_server_response_viewer.php
 *
 *
 ***************************************************************************/

/***************************************************************************
 *
 *   Free Code
 *
 ***************************************************************************/

class dYar_client {
	private $url;
	public function __construct($url) {
		$this->url = $url;
	}

	public function call() {
		if (!func_num_args())
			throw new exception('参数不够');
		$args = func_get_args();
		$m = $args[0];
		array_shift($args);
		return dYar_transports::exec($this->url, dYar_protocol::Package($m, $args));
	}

	public function __call($name, $arguments) {
		return dYar_transports::exec($this->url, dYar_protocol::Package($name, $arguments));
	}
}

class dYar_Concurrent_Client {
	private $data = array();
	public static function call($uri, $m, $params = null, $callback = null, $errorcallback = null) {
		$package = dYar_protocol::Package($m, $params);
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
			$ret = dYar_transports::exec($v['uri'], $v['data']);
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

class dYar_protocol {
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
		$header .= '80dfec60';						//magic_num, default is: 0x80DFEC60
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

class dYar_transports {
	public static function exec($url, $data) {
		$urlinfo = parse_url($url);
		$path = $urlinfo['path'] . (!empty($urlinfo['query']) ? '?' . $urlinfo['query'] : '') . (!empty($urlinfo['fragment']) ? '#' . $urlinfo['fragment'] : '');

		$in = "POST {$path} HTTP/1.1\r\n";
		$in .= "Host: {$urlinfo['host']}\r\n";
		$in .= "Content-Type: application/octet-stream\r\n";
		$in .= "Connection: Close\r\n";
		$in .= "Hostname: {$urlinfo['host']}\r\n";
		$in .= "Content-Length: " . strlen($data['data']) . "\r\n\r\n";

		$address = gethostbyname($urlinfo['host']);

		$socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
		$result = socket_connect($socket, $address, 80);
		socket_write($socket, $in . $data['data'], strlen($in . $data['data']));

		$f_out = '';
		while ($out = socket_read($socket, 2048))
			$f_out .= $out;

		$tmp = explode("\r\n\r\n", $f_out);
		return array (
			'header'	=>	$tmp[0],
			'return'	=>	unserialize(substr($tmp[1], 82 + 8)),
		);
	}
}
