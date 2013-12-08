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
  | Author:  Xinchen Hui <laruence@php.net>                              |
  +----------------------------------------------------------------------+
*/


if (php_sapi_name() !== "cli") {
    die ("This tool only run on CLI\n");
}

if ($argc < 4) {
    die ("Usage: php debug.php uri method \"args, args\"\n");
}

include dirname(__FILE__) . "/yar_debug.php";

list($script, $uri, $method, $args) = $argv;

$client = new Yar_Debug_Client($uri);
$response = $client->call($method, explode(",", $args));

print_r($response);
?>
