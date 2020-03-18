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
#include "ext/standard/php_lcg.h" /* for php_combined_lcg’ */
#include "ext/standard/php_rand.h" /* for php_mt_rand */

#include "php_yar.h"
#include "yar_exception.h"
#include "yar_response.h"
#include "yar_request.h"
#include "yar_packager.h"

yar_request_t *php_yar_request_instance(zend_string *method, zval *params, zval *options) /* {{{ */ {
	yar_request_t *request = ecalloc(1, sizeof(yar_request_t));

	if (!BG(mt_rand_is_seeded)) {
		php_mt_srand(GENERATE_SEED());
	}

	request->id = (long)php_mt_rand();

	request->method = zend_string_copy(method);
	if (params) {
		ZVAL_COPY(&request->parameters, params);
	}
	if (options) {
		ZVAL_COPY(&request->options, options);
	}

	return request;
}
/* }}} */

yar_request_t * php_yar_request_unpack(zval *body) /* {{{ */ {
	yar_request_t *req;
	zval *pzval;
	HashTable *ht;

	req = (yar_request_t *)ecalloc(sizeof(yar_request_t), 1);

	if (IS_ARRAY != Z_TYPE_P(body)) {
		return req;
	}

	ht = Z_ARRVAL_P(body);
	if ((pzval = zend_hash_str_find(ht, "i", sizeof("i") - 1)) != NULL) {
		req->id = zval_get_long(pzval);
	}

	if ((pzval = zend_hash_str_find(ht, "m", sizeof("m") - 1)) != NULL) {
		req->method = zval_get_string(pzval);
	}

	if ((pzval = zend_hash_str_find(ht, "p", sizeof("p") - 1)) != NULL) {
		if (IS_ARRAY != Z_TYPE_P(pzval)) {
			convert_to_array(pzval);
		}
		ZVAL_COPY(&req->parameters, pzval);
	}

	return req;
} /* }}} */

zend_string *php_yar_request_pack(yar_request_t *request, char **msg) /* {{{ */ {
	zval rv;
	zend_array req;
	zend_string *payload;
	char *packager_name = NULL;

	/* @TODO: this is ugly, which needs options stash in request */
	if (IS_ARRAY == Z_TYPE(request->options)) {
		zval *pzval;
		if ((pzval = zend_hash_index_find(Z_ARRVAL(request->options), YAR_OPT_PACKAGER)) && IS_STRING == Z_TYPE_P(pzval)) {
			packager_name = Z_STRVAL_P(pzval);
		}
	}

	zend_hash_init(&req, 8, NULL, NULL, 0);

	ZVAL_LONG(&rv, request->id);
	zend_hash_add(&req, ZSTR_CHAR('i'), &rv);

	ZVAL_STR(&rv, request->method);
	zend_hash_add(&req, ZSTR_CHAR('m'), &rv);
	if (IS_ARRAY == Z_TYPE(request->parameters)) {
		zend_hash_add(&req, ZSTR_CHAR('p'), &request->parameters);
	} else {
		zend_array empty_arr;
		zend_hash_init(&empty_arr, 0, NULL, NULL, 0);
		ZVAL_ARR(&rv, &empty_arr);
		zend_hash_add(&req, ZSTR_CHAR('p'), &rv);
	}

	ZVAL_ARR(&rv, &req);
	if (!(payload = php_yar_packager_pack(packager_name, &rv, msg))) {
		zend_hash_destroy(&req);
		return NULL;
	}

	zend_hash_destroy(&req);

	return payload;
}
/* }}} */

void php_yar_request_destroy(yar_request_t *request) /* {{{ */ {
	if (request->method) {
		zend_string_release(request->method);
	}

	zval_ptr_dtor(&request->parameters);

	zval_ptr_dtor(&request->options);

	efree(request);
}
/* }}} */

int php_yar_request_valid(yar_request_t *req, yar_response_t *response, char **msg) /* {{{ */ {
	response->id = req->id;

	if (!req->method) {
		spprintf(msg, 0, "%s", "need specifical request method");
		return 0;
	}

	if (Z_ISUNDEF(req->parameters)) {
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
