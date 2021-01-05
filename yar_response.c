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
#include "yar_exception.h"
#include "yar_request.h"
#include "yar_response.h"

yar_response_t * php_yar_response_instance() /* {{{ */ {
	yar_response_t *response = ecalloc(1, sizeof(yar_response_t));
	return response;
} /* }}} */

int php_yar_response_bind_request(yar_response_t *response, yar_request_t *request) /* {{{ */ {
    response->id = request->id;
	return 1;
} /* }}} */

void php_yar_response_alter_body(yar_response_t *response, zend_string *body, int method) /* {{{ */ {
	response->out = body;
} /* }}} */

void php_yar_response_set_error(yar_response_t *response, int type, char *message, unsigned len) /* {{{ */ {
	ZVAL_STRINGL(&response->err, message, len);
	response->status = type;
} /* }}} */

void php_yar_response_set_exception(yar_response_t *response, zend_object *ex) /* {{{ */ {
	zval *msg, *code, *file, *line;
	zend_class_entry *ce;
	zval zv, rv;

#if PHP_VERSION_ID < 80000
	ZVAL_OBJ(&zv, ex);
	ce = Z_OBJCE(zv);

	msg = zend_read_property(ce, &zv, ZEND_STRL("message"), 0, &rv);
	code = zend_read_property(ce, &zv, ZEND_STRL("code"), 0, &rv);
	file = zend_read_property(ce, &zv, ZEND_STRL("file"), 0, &rv);
	line = zend_read_property(ce, &zv, ZEND_STRL("line"), 0, &rv);
#else
	ce = ex->ce;

	msg = zend_read_property(ce, ex, ZEND_STRL("message"), 0, &rv);
	code = zend_read_property(ce, ex, ZEND_STRL("code"), 0, &rv);
	file = zend_read_property(ce, ex, ZEND_STRL("file"), 0, &rv);
	line = zend_read_property(ce, ex, ZEND_STRL("line"), 0, &rv);
#endif

	array_init(&response->err);

	Z_TRY_ADDREF_P(msg);
	Z_TRY_ADDREF_P(code);
	Z_TRY_ADDREF_P(file);
	Z_TRY_ADDREF_P(line);

	add_assoc_zval_ex(&response->err, ZEND_STRL("message"), msg);
	add_assoc_zval_ex(&response->err, ZEND_STRL("code"), code);
	add_assoc_zval_ex(&response->err, ZEND_STRL("file"), file);
	add_assoc_zval_ex(&response->err, ZEND_STRL("line"), line);

	add_assoc_str_ex(&response->err, ZEND_STRL("_type"), ce->name);

	response->status = YAR_ERR_EXCEPTION;
} /* }}} */

void php_yar_response_set_retval(yar_response_t *response, zval *retval) /* {{{ */ {
	ZVAL_COPY(&response->retval, retval);
} /* }}} */

void php_yar_response_map_retval(yar_response_t *response, zval *ret) /* {{{ */ {
	if (IS_ARRAY != Z_TYPE_P(ret)) {         
		return;
	} else { 
		zval *pzval;                       
		HashTable *ht = Z_ARRVAL_P(ret);     

		if ((pzval = zend_hash_find(ht, ZSTR_CHAR('i'))) == NULL) {
			return;
		}                                    
		convert_to_long(pzval);            
		response->id = Z_LVAL_P(pzval);    

		if ((pzval = zend_hash_find(ht, ZSTR_CHAR('s'))) == NULL) {
			return;
		}                                    
		convert_to_long(pzval);            
		if ((response->status = Z_LVAL_P(pzval)) == YAR_ERR_OKEY) {
			if ((pzval = zend_hash_find(ht, ZSTR_CHAR('o'))) != NULL) {
				response->out = Z_STR_P(pzval);
				ZVAL_NULL(pzval);          
			}                                
			if ((pzval = zend_hash_find(ht, ZSTR_CHAR('r'))) != NULL) {
				ZVAL_COPY(&response->retval, pzval);
			}                                
		} else if ((pzval = zend_hash_find(ht, ZSTR_CHAR('e'))) != NULL) {
			ZVAL_COPY(&response->err, pzval);
		}                      
	}
}
/* }}} */

void php_yar_response_destroy(yar_response_t *response) /* {{{ */ {
	if (response->out) {
		zend_string_release(response->out);
	}

	zval_ptr_dtor(&response->retval);
	zval_ptr_dtor(&response->err);

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
