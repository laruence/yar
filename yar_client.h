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
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_YAR_CLIENT_H
#define PHP_YAR_CLIENT_H

#define YAR_CLIENT_PROTOCOL_HTTP  1
#define YAR_CLIENT_PROTOCOL_TCP   2
#define YAR_CLIENT_PROTOCOL_UDP   3
#define YAR_CLIENT_PROTOCOL_UNIX  4

typedef struct {
	zend_uchar protocol;
	zend_uchar running;
	zend_string *uri;
	zend_array *parameters;
    void **options;
	zend_array *properties; /* for debug info */
	zend_object std;
} yar_client_object;

#define Z_YARCLIENTOBJ(z)    (php_yar_client_fetch_object(Z_OBJ(z)))
#define Z_YARCLIENTOBJ_P(z)  Z_YARCLIENTOBJ((*z))

static zend_always_inline yar_client_object *php_yar_client_fetch_object(zend_object *obj) {
	return (yar_client_object *)((char*)(obj) - XtOffsetOf(yar_client_object, std));
}

YAR_STARTUP_FUNCTION(client);

#endif	/* PHP_YAR_CLIENT_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
