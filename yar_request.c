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
#if PHP_VERSION_ID < 80400
#include "ext/standard/php_rand.h" /* for php_mt_rand */
#else
#include "ext/random/php_random.h"
#endif

#include "php_yar.h"
#include "yar_exception.h"
#include "yar_response.h"
#include "yar_request.h"
#include "yar_packager.h"

yar_request_t *php_yar_request_instance(zend_string *method, zend_array *parameters, void **options) /* {{{ */ {
	yar_request_t *request = emalloc(sizeof(yar_request_t));

#if PHP_VERSION_ID < 80200
	if (!BG(mt_rand_is_seeded)) {
#else
	if (!RANDOM_G(mt19937_seeded)) {
#endif
		php_mt_srand(GENERATE_SEED());
	}

	request->id = (long)php_mt_rand();

	request->method = zend_string_copy(method);
	request->parameters = zend_array_dup(parameters);
	request->options = options;

	return request;
}
/* }}} */

yar_request_t* php_yar_request_unpack(zval *body) /* {{{ */ {
	zval *v;
	zend_string *k;
	yar_request_t *req;

	req = (yar_request_t *)ecalloc(sizeof(yar_request_t), 1);

	ZEND_ASSERT(Z_TYPE_P(body) == IS_ARRAY);
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(body), k, v) {
		if (EXPECTED(k && ZSTR_LEN(k) == 1)) {
			switch (*ZSTR_VAL(k)) {
				case 'i':
					req->id = zval_get_long(v);
				break;
				case 'm':
					req->method = zval_get_string(v);
				break;
				case 'p':
					if (Z_TYPE_P(v) == IS_ARRAY) {
						req->parameters = zend_array_dup(Z_ARRVAL_P(v));
					} else {
#if PHP_VERSION_ID < 70300
						ALLOC_HASHTABLE(req->parameters);
						zend_hash_init(req->parameters, 0, NULL, NULL, 0);
#else
						req->parameters = (zend_array*)&zend_empty_array;
#endif
					}
				default:
				break;
			}
		}
	} ZEND_HASH_FOREACH_END();

	return req;
} /* }}} */

zend_string *php_yar_request_pack(yar_request_t *request, char **msg) /* {{{ */ {
	zval rv;
	zend_array req;
	zend_string *payload;
	char *packager_name = NULL;

	if (request->options && request->options[YAR_OPT_PACKAGER]) {
		packager_name = (char*)(ZSTR_VAL((zend_string*)request->options[YAR_OPT_PACKAGER]));
	}

	zend_hash_init(&req, 8, NULL, NULL, 0);

	ZVAL_LONG(&rv, request->id);
	zend_hash_add(&req, ZSTR_CHAR('i'), &rv);

	ZVAL_STR(&rv, request->method);
	zend_hash_add(&req, ZSTR_CHAR('m'), &rv);
	if (request->parameters) {
		ZVAL_ARR(&rv, request->parameters);
		zend_hash_add(&req, ZSTR_CHAR('p'), &rv);
	} else {
#if PHP_VERSION_ID < 70300
		array_init(&rv);
		zend_hash_add(&req, ZSTR_CHAR('p'), &rv);
	    /*@TODO: memory leak here */
#else
		ZVAL_EMPTY_ARRAY(&rv);
		zend_hash_add(&req, ZSTR_CHAR('p'), &rv);
#endif
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

	if (request->parameters) {
		zend_array_destroy(request->parameters);
	}

	efree(request);
}
/* }}} */

int php_yar_request_valid(yar_request_t *req, yar_response_t *response, char **msg) /* {{{ */ {
	response->id = req->id;

	if (UNEXPECTED(!req->method)) {
		spprintf(msg, 0, "%s", "need specifical request method");
		return 0;
	}

	if (UNEXPECTED(!req->parameters)) {
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
