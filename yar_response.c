/*
  +----------------------------------------------------------------------+
  | Yar - Light, concurrent RPC framework                             |
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
#include "php_yar.h"
#include "yar_exception.h"
#include "yar_request.h"
#include "yar_response.h"

yar_response_t * php_yar_response_instance(TSRMLS_D) /* {{{ */ {
	yar_response_t *response = ecalloc(sizeof(yar_response_t), 1);

	return response;
} /* }}} */

int php_yar_response_bind_request(yar_response_t *response, yar_request_t *request TSRMLS_DC) /* {{{ */ {
    response->id = request->id;
	return 1;
} /* }}} */

void php_yar_response_alter_body(yar_response_t *response, char *body, uint len, int method TSRMLS_DC) /* {{{ */ {
	response->out = body;
	response->out_len = len;
} /* }}} */

void php_yar_response_set_error(yar_response_t *response, int type, char *message, uint len TSRMLS_DC) /* {{{ */ {
	zval *msg;
	response->status = type;
	MAKE_STD_ZVAL(msg);
	ZVAL_STRINGL(msg, message, len, 0);
	response->err = msg;
} /* }}} */

void php_yar_response_set_exception(yar_response_t *response, zval *ex TSRMLS_DC) /* {{{ */ {
	zval *msg, *code, *file, *line, *ret;
	zend_class_entry *ce;

	ce = Z_OBJCE_P(ex);

	msg = zend_read_property(ce, ex, ZEND_STRL("message"), 0 TSRMLS_CC);
	code = zend_read_property(ce, ex, ZEND_STRL("code"), 0 TSRMLS_CC);
	file = zend_read_property(ce, ex, ZEND_STRL("file"), 0 TSRMLS_CC);
	line = zend_read_property(ce, ex, ZEND_STRL("line"), 0 TSRMLS_CC);

	MAKE_STD_ZVAL(ret);
	array_init(ret);

	Z_ADDREF_P(msg);
	Z_ADDREF_P(code);
	Z_ADDREF_P(file);
	Z_ADDREF_P(line);

	add_assoc_zval_ex(ret, ZEND_STRS("message"), msg);
	add_assoc_zval_ex(ret, ZEND_STRS("code"), code);
	add_assoc_zval_ex(ret, ZEND_STRS("file"), file);
	add_assoc_zval_ex(ret, ZEND_STRS("line"), line);

	add_assoc_string_ex(ret, ZEND_STRS("_type"), ce->name, 1);

	response->status = YAR_ERR_EXCEPTION;
    response->err = ret;
	zval_ptr_dtor(&ex);
} /* }}} */

void php_yar_response_set_retval(yar_response_t *response, zval *retval TSRMLS_DC) /* {{{ */ {
	response->retval = retval;
} /* }}} */

void php_yar_response_dtor(yar_response_t *response TSRMLS_DC) /* {{{ */ {
	if (response->out) {
		efree(response->out);
	}

	if (response->retval) {
		zval_ptr_dtor(&response->retval);
	}

	if (response->err) {
		zval_ptr_dtor(&response->err);
	}

	efree(response);
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
