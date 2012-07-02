/*
  +----------------------------------------------------------------------+
  | Yar - Light, concurrent RPC framework                                |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2011 The PHP Group                                |
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
  |          Zhenyu  Zhang <engineer.zzy@gmail.com>                      |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_YAR_CLIENT_H
#define PHP_YAR_CLIENT_H

#define YAR_CLIENT_PROTOCOL_HTTP  1
#define YAR_CLIENT_PROTOCOL_TCP   2
#define YAR_CLIENT_PROTOCOL_UDP   3
#define YAR_CLIENT_PROTOCOL_UNIX  4


#define YAR_CLIENT_OPT_PACKAGER 		0x833701
#define YAR_CLIENT_OPT_TIMEOUT  		0x833702
#define YAR_CLIENT_OPT_CONNECT_TIMEOUT 	0x833703

YAR_STARTUP_FUNCTION(client);
YAR_SHUTDOWN_FUNCTION(client);

#endif	/* PHP_YAR_CLIENT_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
