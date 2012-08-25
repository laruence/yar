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
  |          Zhenyu  Zhang <zhangzhenyu@php.net>                         |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_MSGPACK

#include "php.h"
#include "php_yar.h"
#include "yar_packager.h"

extern void php_msgpack_serialize(smart_str *buf, zval *val TSRMLS_DC);
extern void php_msgpack_unserialize(zval *return_value, char *str, size_t str_len TSRMLS_DC);

int php_yar_packager_msgpack_pack(yar_packager_t *self, zval *pzval, smart_str *buf, char **msg TSRMLS_DC) /* {{{ */ {
	php_msgpack_serialize(buf, pzval TSRMLS_CC);
	return 1;
} /* }}} */

zval * php_yar_packager_msgpack_unpack(yar_packager_t *self, char *content, size_t len, char **msg TSRMLS_DC) /* {{{ */ {
	zval *return_value;
	MAKE_STD_ZVAL(return_value);
	ZVAL_NULL(return_value);
	php_msgpack_unserialize(return_value, content, len TSRMLS_CC);
	return return_value;
} /* }}} */

yar_packager_t yar_packager_msgpack = {
	"MSGPACK",
	php_yar_packager_msgpack_pack,
    php_yar_packager_msgpack_unpack
};

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
