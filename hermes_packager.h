/*
  +----------------------------------------------------------------------+
  | Hermes - Light, concurrent RPC framework                             |
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

#ifndef PHP_HERMES_PACKAGER_H
#define PHP_HERMES_PACKAGER_H

#include "ext/standard/php_smart_str.h"

#define	HERMES_PACKAGER_PHP      1
#define	HERMES_PACKAGER_JSON     2
#define	HERMES_PACKAGER_MESGPACK 3

#define HERMES_PACKAGER_BUFFER_SIZE   10240

typedef struct _hermes_packager {
	const char *name;
	int  (*pack) (struct _hermes_packager *self, zval *pzval, smart_str *buf, char **msg TSRMLS_DC);
	zval * (*unpack) (struct _hermes_packager *self, char *content, size_t len, char **msg TSRMLS_DC);
} hermes_packager_t;

PHP_HERMES_API int php_hermes_packager_register(hermes_packager_t *packager TSRMLS_DC);
PHP_HERMES_API hermes_packager_t * php_hermes_packager_get(char *name, int nlen TSRMLS_DC);

HERMES_STARTUP_FUNCTION(packager);
HERMES_ACTIVATE_FUNCTION(packager);

extern hermes_packager_t hermes_packager_php;
extern hermes_packager_t hermes_packager_json;
#ifdef ENABLE_MSGPACK
extern hermes_packager_t hermes_packager_msgpack;
#endif

size_t php_hermes_packager_pack(zval *pzval, char **payload, char **msg TSRMLS_DC);
zval * php_hermes_packager_unpack(char *content, size_t len, char **msg TSRMLS_DC);

#endif	/* PHP_HERMES_PACKAGER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
