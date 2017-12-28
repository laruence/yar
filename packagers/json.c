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

#include "php.h"
#include "php_yar.h"
#include "ext/json/php_json.h"
#include "yar_packager.h"

int php_yar_packager_json_pack(const yar_packager_t *self, zval *pzval, smart_str *buf, char **msg) /* {{{ */ {
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 3))
	php_json_encode(buf, pzval);
#else
	php_json_encode(buf, pzval, 0); /* options */
#endif

	return 1;
} /* }}} */

zval * php_yar_packager_json_unpack(const yar_packager_t *self, char *content, size_t len, char **msg, zval *rret) /* {{{ */ {
	zval *return_value;

	php_json_decode(rret, content, len, 1, 512);

	return_value = rret;
	return return_value;
} /* }}} */

const yar_packager_t yar_packager_json = {
	"JSON",
	php_yar_packager_json_pack,
    php_yar_packager_json_unpack
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
