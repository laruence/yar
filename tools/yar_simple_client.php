<?php
/*
 * Yar纯php客户端 调用示例
 * @author Pakey (http://pakey.net)
 * @email pakey@ptcms.com
 */

/**
server端代码
class API {
	public function api($parameter = "", $option = "foo") {
		return $this->client_can_not_see($parameter);
	}
	
	protected function client_can_not_see( $name ) {
		return "hello $name";
	}
}
$service = new Yar_Server(new API());
$service->handle();
*/
include './yar_simple_client.inc';

$apiurl='http://localhost/yar/server.php';
// 简单调用
$client = new Yar_Simple_Client($apiurl);
$result = $client->api("parameter");
var_dump($result);
// 并行调用
Yar_Simple_Concurrent_Client::call($apiurl, 'api', array("Concurrent1"),null);
Yar_Simple_Concurrent_Client::call($apiurl, 'api', array("Concurrent2"),null);
Yar_Simple_Concurrent_Client::call($apiurl, 'api', array("Concurrent3"),'foo');
Yar_Simple_Concurrent_Client::call($apiurl, 'api', array("Concurrent4"),null);
Yar_Simple_Concurrent_Client::call($apiurl, 'api', array("Concurrent5"),null);
Yar_Simple_Concurrent_Client::call($apiurl, 'api', array("Concurrent6"),null);
Yar_Simple_Concurrent_Client::call($apiurl, 'api', array("Concurrent7"),null);
Yar_Simple_Concurrent_Client::loop('okfunc');

function okfunc($retval, $callinfo) {
	if ($callinfo == NULL) {
		echo "现在, 所有的请求都发出去了, 还没有任何请求返回<br />";
	} else {
		echo "这是一个远程调用的返回, 调用的服务名是{$callinfo["method"]},调用的sequence是{$callinfo["sequence"]}<br />";
		var_dump($retval);
	}
}

function foo($retval, $callinfo){
	var_dump('callback:foo--'.$callinfo["method"].'--'.$retval);
}
