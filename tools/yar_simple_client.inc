<?php
/*
 * Yar客户端的纯php实现，基于curl
 * @author Pakey (http://pakey.net)
 * @email pakey@ptcms.com
 * todolist
 *  1、完善异常处理
 * 	2、增加其他数据格式
 *  3、集成debug模块
 */
/***************************************************************************
 *   用法：如果未安装yar洛湛，按照正常yar调用方法进行调用，
 *   如果安装了yar扩展，调用时只是把类名修改为本调试类名（Yar_Client>Yar_Sigmple_Client，Yar_Concurrent_Client->Yar_Sigmple_Concurrent_Client）
 *   1
 *   $yar = new Yar_Sigmple_Client('http://yar_server/path');
 *   var_dump($yar->call('method', $params));
 *   2
 *   Yar_Sigmple_Concurrent_Client::call('http://yar_server/path', 'method', $params, $callback, $errcallback);
 *   Yar_Sigmple_Concurrent_Client::loop();
 *
 ***************************************************************************/
if (!class_exists('Yar_Client')){
	class Yar_Client extends Yar_Simple_Client{};
	class Yar_Concurrent_Client extends Yar_Simple_Concurrent_Client{};
}
/**
 * Yar_Client 的实现
 * Class Yar_Simple_Client
 */
class Yar_Simple_Client
{
	protected $_uri;
	protected $_options;

	public function __call($method, array $parameters)
	{
		return Yar_Simple_Transport::exec($this->_uri, Yar_Simple_Protocol::Pack($method, $parameters));
	}

	final public function __construct($url)
	{
		$this->_uri = $url;
	}

	// todo 参数设置
	public function setOpt($name, $value)
	{

	}
}

/**
 * Yar_Concurrent_Client 的实现
 * Class Yar_Concurrent_Client
 */
class Yar_Simple_Concurrent_Client
{
	static $_callstack;
	static $_callback;
	static $_error_callback;

	public static function call($uri, $method, $parameters, $callback = null, $error_callback = null, $opt = array())
	{
		Yar_Simple_Transport::add($uri, $method, $parameters, $callback, $error_callback, $opt);
	}

	public static function loop($callback = null, $error_callback = null)
	{
		Yar_Simple_Transport::go($callback, $error_callback);
	}
}

class Yar_Simple_Transport
{
	protected static $task=array();
	protected static $taskRunning=array();
	protected static $mh;

	/**
	 * 简单的客户端调用
	 * @param $url
	 * @param $data
	 * @return mixed
	 */
	public static function exec($url, $data)
	{
		$ch = self::init($url,$data);
		$content = curl_exec($ch);
		$runinfo = curl_getinfo($ch);
		if ($runinfo['http_code'] === 200) {
			return Yar_Simple_Protocol::unpack($content);
		} else {
			//todo 请求失败
		}
	}

	/**
	 * Yar_Concurrent_Client::call的实现 — 注册一个并行的服务调用
	 * @param $uri
	 * @param $method
	 * @param $parameters
	 * @param null $callback
	 * @param null $error_callback
	 * @param array $opt
	 * @throws Exception
	 */
	public static function add($uri, $method, $parameters, $callback = null, $error_callback = null, $opt = array())
	{
		// 参数校验
		if ($callback !== null && !function_exists($callback)) {
			throw new Exception('fourth parameter is expected to be a valid callback');
		}
		if ($error_callback !== null && !function_exists($error_callback))
			throw new Exception('fifth parameter is expected to be a valid callback');
		// opt处理
		$opt=self::parseOpt($opt);
		// 数据记录
		self::$task[] = array(
			'uri' => $uri,
			'sequence'=>count(self::$task)+1,
			'method' => $method,
			'data' => Yar_Simple_Protocol::Pack($method, $parameters),
			'callback' => $callback,
			'error_callback' => $error_callback,
			'opt' => $opt,
		);
	}

	/**
	 * Yar_Concurrent_Client::loop的实现 — 发送所有注册的并行调用
	 * @param null $callback
	 * @param null $error_callback
	 * @throws Exception
	 */
	public static function go($callback = null, $error_callback = null)
	{
		// 参数校验
		if ($callback !== null && !function_exists($callback)) {
			throw new Exception('first parameter is expected to be a valid callback');
		}
		if ($error_callback !== null && !function_exists($error_callback))
			throw new Exception('second parameter is expected to be a valid callback');
		//
		self::$mh = curl_multi_init();
		foreach (self::$task as $task) {
			//设置成功及失败回调
			$task['callback']=($task['callback']===null && $callback)?$callback:$task['callback'];
			$task['error_callback']=($task['error_callback']===null && $error_callback)?$error_callback:$task['error_callback'];
			if ($task['callback']===null) throw new Exception('success callback must be set!');
			// curl初始化
			$ch = self::init($task['uri'],$task['data']);
			if (is_resource($ch)) {
				if (isset($task['opt'])) {
					foreach ($task['opt'] as $k => $v)
						curl_setopt($ch, $k, $v);
				}
				curl_multi_add_handle(self::$mh, $ch);
			}
			self::$taskRunning[intval($ch)] = $task;
		}
		self::$task=array();
		// 所有请求发出 调用回调
		if ($callback) call_user_func_array($callback,array(null,null));
		// 并行处理
		do {
			while (curl_multi_exec(self::$mh, $activeNum) === CURLM_CALL_MULTI_PERFORM) {}
			curl_multi_select(self::$mh); //
			while ($curlInfo = curl_multi_info_read(self::$mh, $queueNum)) {
				$ch = $curlInfo['handle'];
				$info = curl_getinfo($ch);
				$id = intval($ch);
				$task = self::$taskRunning[$id];
				unset(self::$taskRunning[$id]);
				if ($curlInfo['result'] == CURLE_OK && $info['http_code']='200') {
					// 成功
					$retval=Yar_Simple_Protocol::unpack(curl_multi_getcontent($ch));
					$callinfo=array(
						'sequence'=>$task['sequence'],
						'method'=>$task['method'],
						'uri'=>$task['uri'],
					);
					call_user_func_array($task['callback'], array($retval,$callinfo));
				} else {
					// todo 失败回调
					//call_user_func_array($task['error_callback'], array($type, $error, $callinfo));
				}
				curl_multi_remove_handle(self::$mh, $ch);
				curl_close($ch);
			}
		} while (!empty(self::$taskRunning));
		curl_multi_close(self::$mh);
		self:$mh=null;
	}

	// 初始化
	private static function init($url,$data)
	{
		$ch = curl_init();
		$opt = array();
		$opt[CURLOPT_URL] = $url;
		$opt[CURLOPT_HEADER] = 0;
		$opt[CURLOPT_RETURNTRANSFER] = 1;
		$opt[CURLOPT_POST] = 1;
		$opt[CURLOPT_POSTFIELDS] = $data;
		curl_setopt_array($ch, $opt);
		return $ch;
	}

	// todo opt处理为curl可用参数
	protected static function parseOpt($opt){
		return array();
	}
}

/**
 * Class Yar_Simple_Protocol
 */
class Yar_Simple_Protocol
{
	/**
	 * from Yar_Simple_Protocol
	 *
	 * @param $method
	 * @param $params
	 * @return array
	 */
	public static function pack($method, $params)
	{
		$struct = array(
			'i' => time(),
			'm' => $method,
			'p' => $params,
		);
		$body = str_pad('PHP', 8, chr(0)) . serialize($struct);
		$transaction = sprintf('%08x', mt_rand());
		$header = $transaction; //transaction id
		$header .= sprintf('%04x', 0); //protocl version
		$header .= '80DFEC60'; //magic_num, default is: 0x80DFEC60
		$header .= sprintf('%08x', 0); //reserved
		$header .= sprintf('%064x', 0); //reqeust from who
		$header .= sprintf('%064x', 0); //request token, used for authentication
		$header .= sprintf('%08x', strlen($body)); //request body len
		$data = '';
		for ($i = 0; $i < strlen($header); $i = $i + 2)
			$data .= chr(hexdec('0x' . $header[$i] . $header[$i + 1]));
		$data .= $body;
		return $data;
	}

	/**
	 * curl结果解析
	 * @param $con
	 * @return mixed
	 * @throws Exception
	 */
	public static function unpack($con)
	{
		$ret = unserialize(substr($con, 82 + 8));
		/**
		 *    array(
		 *        "i" => '', //time
		 *        "s" => '', //status
		 *        "r" => '', //return value
		 *        "o" => '', //output
		 *        "e" => '', //error or exception
		 *    )
		 */
		if ($ret['s'] === 0) {
			return $ret['r'];
		} elseif (is_array($ret)) {
			throw new Exception($ret['e']);
		} else {
			throw new Exception('malformed response header');
		}
	}

}