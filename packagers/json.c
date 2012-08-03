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

#include "php.h"
#include "php_yar.h"
#include "yar_packager.h"

int php_yar_packager_json_pack(yar_packager_t *self, zval *pzval, smart_str *buf, char **msg TSRMLS_DC) /* {{{ */ {
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 3))
	php_json_encode(buf, pzval TSRMLS_CC);
#else
	php_json_encode(buf, pzval, 0 TSRMLS_CC); /* options */
#endif

	return 1;
} /* }}} */

zval * php_yar_packager_json_unpack(yar_packager_t *self, char *content, size_t len, char **msg TSRMLS_DC) /* {{{ */ {
	zval *return_value;
	MAKE_STD_ZVAL(return_value);

#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 3))
	php_json_decode(return_value, content, len, 1 TSRMLS_CC);
#else
	php_json_decode(return_value, content, len, 1, 512 TSRMLS_CC);
#endif

	return return_value;
} /* }}} */

yar_packager_t yar_packager_json = {
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
