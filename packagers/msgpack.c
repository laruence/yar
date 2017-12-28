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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_MSGPACK

#include "php.h"
#include "php_yar.h"
#include "yar_packager.h"

extern void php_msgpack_serialize(smart_str *buf, zval *val);
extern void php_msgpack_unserialize(zval *return_value, char *str, size_t str_len);

int php_yar_packager_msgpack_pack(const yar_packager_t *self, zval *pzval, smart_str *buf, char **msg) /* {{{ */ {
	php_msgpack_serialize(buf, pzval);
	return 1;
} /* }}} */

zval * php_yar_packager_msgpack_unpack(const yar_packager_t *self, char *content, size_t len, char **msg, zval *rret) /* {{{ */ {
	zval *return_value;
	ZVAL_NULL(rret);
	php_msgpack_unserialize(rret, content, len);
	return_value = rret;
	return return_value;
} /* }}} */

const yar_packager_t yar_packager_msgpack = {
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
