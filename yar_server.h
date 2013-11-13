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
  | Author:  Xinchen Hui   <laruence@php.net>                            |
  |          Zhenyu  Zhang <zhangzhenyu@php.net>                         |
  |                                                                      |
  | Fork:  Qinghui Zeng  <zengohm@gmail.com>                             |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_YAR_SERVER_H
#define PHP_YAR_SERVER_H

#define YAR_SERVER_PROTOCOL_HTTP  1
#define YAR_SERVER_PROTOCOL_TCP   2

YAR_STARTUP_FUNCTION(service);
YAR_SHUTDOWN_FUNCTION(service);

#endif	/* PHP_YAR_SERVER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
