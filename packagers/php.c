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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_yar.h"
#include "yar_packager.h"
#include "ext/standard/php_var.h" /* for serialize */

int php_yar_packager_php_pack(const yar_packager_t *self, zval *pzval, smart_str *buf, char **msg) /* {{{ */ {
	php_serialize_data_t var_hash;

	PHP_VAR_SERIALIZE_INIT(var_hash);
	php_var_serialize(buf, pzval, &var_hash);
	PHP_VAR_SERIALIZE_DESTROY(var_hash);

	return 1;
} /* }}} */

zval * php_yar_packager_php_unpack(const yar_packager_t *self, char *content, size_t len, char **msg, zval *rret) /* {{{ */ {
	zval *return_value;
	const unsigned char *p;
	php_unserialize_data_t var_hash;
	p = (const unsigned char*)content;

	PHP_VAR_UNSERIALIZE_INIT(var_hash);
	if (!php_var_unserialize(rret, &p, p + len,  &var_hash)) {
		zval_ptr_dtor(rret);
		PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
		spprintf(msg, 0, "unpack error at offset %ld of %ld bytes", (long)((char*)p - content), len);
		return NULL;
	}
	PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

	return_value = rret;
	return return_value;
} /* }}} */

const yar_packager_t yar_packager_php = {
	"PHP",
	php_yar_packager_php_pack,
    php_yar_packager_php_unpack
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
