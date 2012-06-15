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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_hermes.h"
#include "hermes_exception.h"
#include "hermes_response.h"
#include "hermes_request.h"

hermes_request_t * php_hermes_request_instance(zval *body TSRMLS_DC) /* {{{ */ {
	hermes_request_t *req;
	zval **ppzval;
	HashTable *ht;

    req = (hermes_request_t *)ecalloc(sizeof(hermes_request_t), 1);

	if (IS_ARRAY != Z_TYPE_P(body)) {
		return req;
	}

	ht = Z_ARRVAL_P(body);
	if (zend_hash_find(ht, "i", sizeof("i"), (void**)&ppzval) == SUCCESS) {
		if (IS_LONG != Z_TYPE_PP(ppzval)) {
			convert_to_long_ex(ppzval);
			req->id = Z_LVAL_PP(ppzval);
		} else {
			req->id = Z_LVAL_PP(ppzval);
		}
	}

	if (zend_hash_find(ht, "m", sizeof("m"), (void**)&ppzval) == SUCCESS) {
		if (IS_STRING != Z_TYPE_PP(ppzval)) {
           convert_to_string_ex(ppzval);
		}
		MAKE_STD_ZVAL(req->method);
		ZVAL_STRINGL(req->method, Z_STRVAL_PP(ppzval), Z_STRLEN_PP(ppzval), 1);
	}

	if (zend_hash_find(ht, "p", sizeof("p"), (void**)&ppzval) == SUCCESS) {
		if (IS_ARRAY != Z_TYPE_PP(ppzval)) {
			convert_to_array_ex(ppzval);
		}
		MAKE_STD_ZVAL(req->parameters);
		array_init(req->parameters);

		zend_hash_copy(Z_ARRVAL_P(req->parameters), Z_ARRVAL_PP(ppzval), (copy_ctor_func_t)zval_add_ref, NULL, sizeof(zval *));
	}

	return req;
} /* }}} */

int php_hermes_request_valid(hermes_request_t *req, hermes_response_t *response TSRMLS_DC) /* {{{ */ {
	if (!req->id) {
       php_hermes_error(response, HERMES_ERR_REQUEST, "no specifical request id");
	   return 0;
	}

	if (!req->method) {
       php_hermes_error(response, HERMES_ERR_REQUEST, "no specifical request method");
	   return 0;
	}

	if (!req->parameters) {
       php_hermes_error(response, HERMES_ERR_REQUEST, "no specifical request parameters");
	   return 0;
	}

	return 1;
} /* }}} */

void php_hermes_request_dtor(hermes_request_t *req TSRMLS_DC) /* {{{ */ {
	if (req->method) {
		zval_ptr_dtor(&req->method);
	}

	if (req->header) {
		zval_ptr_dtor(&req->header);
	}

	if (req->parameters) {
		zval_ptr_dtor(&req->parameters);
	}

	efree(req);
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
