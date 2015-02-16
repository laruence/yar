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

#ifndef PHP_YAR_PACKAGER_H
#define PHP_YAR_PACKAGER_H

#include "Zend/zend_smart_str.h"

#define	YAR_PACKAGER_PHP      "PHP"
#define	YAR_PACKAGER_JSON     "JSON"
#define	YAR_PACKAGER_MSGPACK "MSGPACK"

#define YAR_PACKAGER_BUFFER_SIZE  5120

typedef struct _yar_packager {
	const char *name;
	int  (*pack) (struct _yar_packager *self, zval *pzval, smart_str *buf, char **msg);
	zval * (*unpack) (struct _yar_packager *self, char *content, size_t len, char **msg, zval *rret);
} yar_packager_t;

PHP_YAR_API int php_yar_packager_register(yar_packager_t *packager);
PHP_YAR_API yar_packager_t * php_yar_packager_get(char *name, int nlen);

YAR_STARTUP_FUNCTION(packager);
YAR_ACTIVATE_FUNCTION(packager);

extern yar_packager_t yar_packager_php;
extern yar_packager_t yar_packager_json;
#ifdef ENABLE_MSGPACK
extern yar_packager_t yar_packager_msgpack;
#endif

size_t php_yar_packager_pack(char *packager_name, zval *pzval, zend_string **payload, char **msg);
zval * php_yar_packager_unpack(char *content, size_t len, char **msg, zval *rret);

YAR_STARTUP_FUNCTION(packager);
YAR_ACTIVATE_FUNCTION(packager);
YAR_SHUTDOWN_FUNCTION(packager);

#endif	/* PHP_YAR_PACKAGER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
