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

#ifndef PHP_YAR_PACKAGER_H
#define PHP_YAR_PACKAGER_H

#include "ext/standard/php_smart_str.h"

#define	YAR_PACKAGER_PHP      1
#define	YAR_PACKAGER_JSON     2
#define	YAR_PACKAGER_MESGPACK 3

#define YAR_PACKAGER_BUFFER_SIZE  5120

typedef struct _yar_packager {
	const char *name;
	int  (*pack) (struct _yar_packager *self, zval *pzval, smart_str *buf, char **msg TSRMLS_DC);
	zval * (*unpack) (struct _yar_packager *self, char *content, size_t len, char **msg TSRMLS_DC);
} yar_packager_t;

PHP_YAR_API int php_yar_packager_register(yar_packager_t *packager TSRMLS_DC);
PHP_YAR_API yar_packager_t * php_yar_packager_get(char *name, int nlen TSRMLS_DC);

YAR_STARTUP_FUNCTION(packager);
YAR_ACTIVATE_FUNCTION(packager);

extern yar_packager_t yar_packager_php;
extern yar_packager_t yar_packager_json;
#ifdef ENABLE_MSGPACK
extern yar_packager_t yar_packager_msgpack;
#endif

size_t php_yar_packager_pack(zval *pzval, char **payload, char **msg TSRMLS_DC);
zval * php_yar_packager_unpack(char *content, size_t len, char **msg TSRMLS_DC);

#endif	/* PHP_YAR_PACKAGER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
