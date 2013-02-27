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
#include "ext/standard/php_lcg.h" /* for php_combined_lcg’ */
#include "ext/standard/php_rand.h" /* for php_mt_rand */

#include "php_yar.h"
#include "yar_exception.h"
#include "yar_response.h"
#include "yar_request.h"
#include "yar_packager.h"

yar_request_t * php_yar_request_instance(char *method, long mlen, zval *params, zval *options TSRMLS_DC) /* {{{ */ {
	yar_request_t *request = ecalloc(1, sizeof(yar_request_t));

	if (!BG(mt_rand_is_seeded)) {
		php_mt_srand(GENERATE_SEED() TSRMLS_CC);
	}

	request->id = (long)php_mt_rand(TSRMLS_C);

	request->method = estrndup(method, mlen);
	request->mlen   = mlen;
	if (params) {
		Z_ADDREF_P(params);
		request->parameters = params;
	}
	if (options) {
		Z_ADDREF_P(options);
		request->options = options;
	}

	return request;
}
/* }}} */

yar_request_t * php_yar_request_unpack(zval *body TSRMLS_DC) /* {{{ */ {
	yar_request_t *req;
	zval **ppzval;
	HashTable *ht;

	req = (yar_request_t *)ecalloc(sizeof(yar_request_t), 1);

	if (IS_ARRAY != Z_TYPE_P(body)) {
		return req;
	}

	ht = Z_ARRVAL_P(body);
	if (zend_hash_find(ht, "i", sizeof("i"), (void**)&ppzval) == SUCCESS) {
		if (IS_LONG != Z_TYPE_PP(ppzval)) {
			convert_to_long(*ppzval);
		}
		req->id = Z_LVAL_PP(ppzval);
	}

	if (zend_hash_find(ht, "m", sizeof("m"), (void**)&ppzval) == SUCCESS) {
		if (IS_STRING != Z_TYPE_PP(ppzval)) {
			convert_to_string(*ppzval);
		}
		req->method = Z_STRVAL_PP(ppzval);
		req->mlen = Z_STRLEN_PP(ppzval);
		ZVAL_NULL(*ppzval);
	}

	if (zend_hash_find(ht, "p", sizeof("p"), (void**)&ppzval) == SUCCESS) {
		if (IS_ARRAY != Z_TYPE_PP(ppzval)) {
			convert_to_array(*ppzval);
		}
		Z_ADDREF_P(*ppzval);
		req->parameters = *ppzval;
	}

	return req;
} /* }}} */

zval * php_yar_request_pack(yar_request_t *request, char **msg TSRMLS_DC) /* {{{ */ {
	zval zreq, *ret;
	char *payload, *packager_name = NULL;
	size_t payload_len;

	/* @TODO: this is ugly, which needs options stash in request */
	if (request->options && IS_ARRAY == Z_TYPE_P(request->options)) {
		zval **ppzval;
		if (zend_hash_index_find(Z_ARRVAL_P(request->options), YAR_OPT_PACKAGER, (void **)&ppzval) == SUCCESS
				&& IS_STRING == Z_TYPE_PP(ppzval)) {
			packager_name = Z_STRVAL_PP(ppzval);
		}
	}

	INIT_ZVAL(zreq);
	array_init(&zreq);

	add_assoc_long_ex(&zreq, ZEND_STRS("i"), request->id);
	add_assoc_stringl_ex(&zreq, ZEND_STRS("m"), request->method, request->mlen, 1);

	if (request->parameters) {
		Z_ADDREF_P(request->parameters);
		add_assoc_zval_ex(&zreq, ZEND_STRS("p"), request->parameters);
	} else {
		zval *tmp;
		MAKE_STD_ZVAL(tmp);
		array_init(tmp);
		add_assoc_zval_ex(&zreq, ZEND_STRS("p"), tmp);
	}

	if (!(payload_len = php_yar_packager_pack(packager_name, &zreq, &payload, msg TSRMLS_CC))) {
		zval_dtor(&zreq);
		return NULL;
	}

	zval_dtor(&zreq);

	MAKE_STD_ZVAL(ret);
	ZVAL_STRINGL(ret, payload, payload_len, 0);

	return ret;
}
/* }}} */

void php_yar_request_destroy(yar_request_t *request TSRMLS_DC) /* {{{ */ {
	if (request->method) {
		efree(request->method);
	}

	if (request->parameters) {
		zval_ptr_dtor(&request->parameters);
	}

	if (request->options) {
		zval_ptr_dtor(&request->options);
	}

	efree(request);
}
/* }}} */

int php_yar_request_valid(yar_request_t *req, yar_response_t *response, char **msg TSRMLS_DC) /* {{{ */ {
	response->id = req->id;

	if (!req->method) {
		spprintf(msg, 0, "%s", "need specifical request method");
		return 0;
	}

	if (!req->parameters) {
		spprintf(msg, 0, "%s", "need specifical request parameters");
		return 0;
	}

	return 1;
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
