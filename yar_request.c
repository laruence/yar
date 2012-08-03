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
#include "yar_exception.h"
#include "yar_response.h"
#include "yar_request.h"

yar_request_t * php_yar_request_instance(zval *body TSRMLS_DC) /* {{{ */ {
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
		Z_ADDREF_P(*ppzval);
		req->method = *ppzval;
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

int php_yar_request_valid(yar_request_t *req, yar_response_t *response TSRMLS_DC) /* {{{ */ {
	if (!req->id) {
       php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "no specifical request id");
	   return 0;
	}

	if (!req->method) {
       php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "no specifical request method");
	   return 0;
	}

	if (!req->parameters) {
       php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "no specifical request parameters");
	   return 0;
	}

	return 1;
} /* }}} */

void php_yar_request_dtor(yar_request_t *req TSRMLS_DC) /* {{{ */ {
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
