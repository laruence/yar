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
  |          Zhenyu  Zhang <engineer.zzy@gmail.com>                      |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "SAPI.h"
#include "php_yar.h"
#include "yar_exception.h"
#include "yar_packager.h"
#include "yar_transport.h"
#include "yar_client.h"
#include "yar_request.h"
#include "yar_response.h"
#include "yar_protocol.h"
#include "ext/standard/php_rand.h" /* for php_mt_rand */
#include "ext/standard/url.h" /* for php_url */

zend_class_entry *yar_client_ce;
zend_class_entry *yar_concurrent_client_ce;

/* {{{ ARG_INFO */
ZEND_BEGIN_ARG_INFO_EX(arginfo_client___construct, 0, 0, 1)
	ZEND_ARG_INFO(0, url)
	ZEND_ARG_INFO(0, async)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(arginfo_client___call, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, parameters)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(arginfo_client_async, 0, 0, 3)
	ZEND_ARG_INFO(0, uri)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, parameters)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(arginfo_client_loop, 0, 0, 0)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()
/* }}} */

static void php_yar_client_trigger_error(int throw_exception TSRMLS_DC, int code, const char *format, ...) /* {{{ */ {
	va_list arg;
	char *message;
	zend_class_entry *ce;

	va_start(arg, format);
	zend_vspprintf(&message, 0, format, arg);
	va_end(arg);

	if (throw_exception) {
		zval *ex;
		MAKE_STD_ZVAL(ex);
		switch (code) {
			case YAR_ERR_PACKAGER:
				ce = yar_client_packager_exception_ce;
				break;
			case YAR_ERR_PROTOCOL:
				ce = yar_client_protocol_exception_ce;
				break;
			case YAR_ERR_TRANSPORT:
				ce = yar_client_transport_exception_ce;
				break;
			default:
				ce  = yar_client_exception_ce;
				break;
		}
		zend_throw_exception(ce, message, code TSRMLS_CC);
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "[%d] %s", code, message);
	}

	efree(message);
} /* }}} */

static zval * php_yar_client_parse_response(char *ret, size_t len, int throw_exception TSRMLS_DC) /* {{{ */ {
	zval *retval, *response;
	yar_header_t *header;
	char *err_msg;

	MAKE_STD_ZVAL(retval);
	ZVAL_FALSE(retval);

	if (!(header = php_yar_protocol_parse(&ret, &len, &err_msg TSRMLS_CC))) {
		php_yar_client_trigger_error(throw_exception TSRMLS_CC, YAR_ERR_PROTOCOL, "%s", err_msg);
		if (YAR_G(debug)) {
			php_yar_debug_client("0: malformed response '%s'", ret);
		}
		efree(err_msg);
		return retval;
	}

	if (!len || !header->body_len) {
		php_yar_client_trigger_error(throw_exception TSRMLS_CC, 0, "server responsed empty body");
		return retval;
	}

	if (YAR_G(debug)) {
		php_yar_debug_client("%ld: server responsed: packager '%s', len '%ld', content '%s'", header->id, ret, len - 8, ret + 8);
	}

	if (!(response = php_yar_packager_unpack(ret, len, &err_msg TSRMLS_CC))) {
		php_yar_client_trigger_error(throw_exception TSRMLS_CC, YAR_ERR_PACKAGER, "%s", err_msg);
		efree(err_msg);
		return retval;
	}

	if (response && IS_ARRAY == Z_TYPE_P(response)) {
		zval **ppzval;
		uint status;
		HashTable *ht = Z_ARRVAL_P(response);
	    if (zend_hash_find(ht, ZEND_STRS("s"), (void **)&ppzval) == FAILURE) {
		}

	    convert_to_long(*ppzval);

		status = Z_LVAL_PP(ppzval);
		if (status == YAR_ERR_OKEY) {
			if (zend_hash_find(ht, ZEND_STRS("o"), (void **)&ppzval) == SUCCESS) {
				PHPWRITE(Z_STRVAL_PP(ppzval), Z_STRLEN_PP(ppzval));
			}
		} else if (throw_exception && status == YAR_ERR_EXCEPTION) {
			if (zend_hash_find(ht, ZEND_STRS("e"), (void **)&ppzval) == SUCCESS) {
				zval *ex, **property;
				MAKE_STD_ZVAL(ex);
				object_init_ex(ex, yar_server_exception_ce);

				if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("message"), (void **)&property) == SUCCESS) {
					zend_update_property(yar_server_exception_ce, ex, ZEND_STRL("message"), *property TSRMLS_CC);
				}

				if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("code"), (void **)&property) == SUCCESS) {
					zend_update_property(yar_server_exception_ce, ex, ZEND_STRL("code"), *property TSRMLS_CC);
				}

				if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("file"), (void **)&property) == SUCCESS) {
					zend_update_property(yar_server_exception_ce, ex, ZEND_STRL("file"), *property TSRMLS_CC);
				}

				if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("line"), (void **)&property) == SUCCESS) {
					zend_update_property(yar_server_exception_ce, ex, ZEND_STRL("line"), *property TSRMLS_CC);
				}

				if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("_type"), (void **)&property) == SUCCESS) {
					zend_update_property(yar_server_exception_ce, ex, ZEND_STRL("_type"), *property TSRMLS_CC);
				}

				zend_throw_exception_internal(ex TSRMLS_CC);
			}
		} else if (zend_hash_find(ht, ZEND_STRS("e"), (void **)&ppzval) == SUCCESS
				&& IS_STRING == Z_TYPE_PP(ppzval)) {
			php_yar_client_trigger_error(throw_exception TSRMLS_CC, status, "%s", Z_STRVAL_PP(ppzval));
		}
		if (zend_hash_find(ht, ZEND_STRS("r"), (void **)&ppzval) == SUCCESS) {
			ZVAL_ZVAL(retval, *ppzval, 1, 0);
		} 
		zval_ptr_dtor(&response);
	} else if (response) {
		zval_ptr_dtor(&response);
	}

	return retval;
} /* }}} */

static int php_yar_client_http_prepare(yar_transport_interface_t *transport, char *uri, long ulen, char *method, long mlen, zval *params TSRMLS_DC) /* {{{ */ {
	char *payload, *err_msg;
	size_t payload_len;
	zval request;
	yar_header_t header = {0};
	long request_id;
	php_url *url;
   
	if (!BG(mt_rand_is_seeded)) {
		php_mt_srand(GENERATE_SEED() TSRMLS_CC);
	}
    request_id = (long)php_mt_rand(TSRMLS_C);

	INIT_ZVAL(request);
	array_init(&request);

	add_assoc_long_ex(&request, ZEND_STRS("i"), request_id);
	add_assoc_stringl_ex(&request, ZEND_STRS("m"), method, mlen, 1);
    add_assoc_zval_ex(&request, ZEND_STRS("p"), params);


	if (YAR_G(debug)) {
		php_yar_debug_client("%ld: call api '%s' at '%s' with '%d parameters'",
				request_id, method, uri, zend_hash_num_elements(Z_ARRVAL_P(params)));
	}

	if (!(payload_len = php_yar_packager_pack(&request, &payload, &err_msg TSRMLS_CC))) {
		zval_dtor(&request);
		php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_PACKAGER, "%s", err_msg);
		efree(err_msg);
		return 0;
	}

	zval_dtor(&request);

	if (YAR_G(debug)) {
		php_yar_debug_client("%ld: package result: packager '%s', len: '%ld', content '%s'",
				request_id, payload, payload_len - 8, payload + 8);
	}

	if (!(url = php_url_parse(uri))) {
		efree(payload);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "malformed uri: '%s'", uri);
		return 0;
	}

	transport->open(transport, uri, ulen, url->host, 0 TSRMLS_CC);

	php_yar_protocol_render(&header, request_id, url->user, url->pass, payload_len, 0 TSRMLS_CC);

	php_url_free(url);

    transport->send(transport, (char *)&header, sizeof(yar_header_t) TSRMLS_CC);
    transport->send(transport, payload, payload_len TSRMLS_CC);
	efree(payload);

	return 1;
} /* }}} */

static zval * php_yar_client_http_handle(zval *client, char *method, long mlen, zval *params TSRMLS_DC) /* {{{ */ {
	zval *uri, *response = NULL;
	yar_transport_t *factory;
	yar_transport_interface_t *transport;
	char *ret, *err_msg;
	size_t ret_len;
	uint err_code;

	uri = zend_read_property(yar_client_ce, client, ZEND_STRL("_uri"), 0 TSRMLS_CC);

	factory = php_yar_transport_get(ZEND_STRL("curl") TSRMLS_CC);
	transport = factory->init(TSRMLS_C);

 	if (!php_yar_client_http_prepare(transport, Z_STRVAL_P(uri), Z_STRLEN_P(uri), method, mlen, params TSRMLS_CC)) {
		transport->close(transport TSRMLS_CC);
		factory->destroy(transport TSRMLS_CC);
        return NULL;
	}

	if (transport->exec(transport, &ret, &ret_len, &err_code, &err_msg TSRMLS_CC)) {
		if (ret_len) {
			response = php_yar_client_parse_response(ret, ret_len, 1 TSRMLS_CC);
			efree(ret);
		} else{
			php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_PROTOCOL, "%s", "server responsed empty response");
		}
	} else {
		php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_TRANSPORT, err_msg);
		efree(err_msg);
	}

	transport->close(transport TSRMLS_CC);
	factory->destroy(transport TSRMLS_CC);
	return response;
} /* }}} */

int php_yar_concurrent_client_callback(zval *calldata, void *gcallback, char *ret, size_t len TSRMLS_DC) /* {{{ */ {
	zval **callback, **method, **uri, **sequence;
	zval *response = NULL, *retval_ptr = NULL;
	zval ***func_params;
	zval *callback1 = (zval *)gcallback;
	zend_bool bailout = 0;

	if (zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("c"), (void **)&callback) == FAILURE) {
		callback = &callback1;
	}

	if (!(*callback) || ZVAL_IS_NULL(*callback)) {
		if (len) {
			efree(ret);
		}
		return 1;
	}

	if (len) {
		response = php_yar_client_parse_response(ret, len, 0 TSRMLS_CC);
		efree(ret);
	} else {
		php_yar_client_trigger_error(0 TSRMLS_CC, YAR_ERR_PROTOCOL, "%s", "server responsed empty response");
		return 1;
	}

	zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("u"), (void **)&uri);
	zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("m"), (void **)&method);
	zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("i"), (void **)&sequence);

	func_params = emalloc(sizeof(zval **) * 4);
	func_params[0] = &response;
	func_params[1] = sequence;
	func_params[2] = method;
	func_params[3] = uri;

	zend_try {
		if (call_user_function_ex(EG(function_table), NULL, *callback, &retval_ptr, 4, func_params, 0, NULL TSRMLS_CC) != SUCCESS) {
			zval_ptr_dtor(&response);
			efree(func_params);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "call to callback failed for request: %s", Z_STRVAL_PP(method));
			return;
		}
	} zend_catch {
		bailout = 1;
	} zend_end_try();

	zval_ptr_dtor(&response);

	if (retval_ptr) {
		zval_ptr_dtor(&retval_ptr);
	}

	efree(func_params);
    return bailout? 0 : 1;
} /* }}} */

int php_yar_concurrent_client_error_callback(zval *calldata, void *error_callback, int error_code, char *err_msg TSRMLS_DC) /* {{{ */ {
	zval ***func_params, **uri, **method, **sequence, *err, *code;
	zval *retval_ptr = NULL, *callback = (zval *)error_callback;
	zend_bool bailout = 0;

	if (!callback) {
		return 1;
	}

	MAKE_STD_ZVAL(err);
	MAKE_STD_ZVAL(code);

	ZVAL_LONG(code, error_code);
	ZVAL_STRING(err, err_msg, 1);

	zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("u"), (void **)&uri);
	zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("m"), (void **)&method);
	zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("i"), (void **)&sequence);

	func_params = emalloc(sizeof(zval **) * 5);
	func_params[0] = sequence;
	func_params[1] = method;
	func_params[2] = uri;
	func_params[3] = &code;
	func_params[4] = &err;

	zend_try {
		if (call_user_function_ex(EG(function_table), NULL, callback, &retval_ptr, 5, func_params, 0, NULL TSRMLS_CC) != SUCCESS) {
			zval_ptr_dtor(&err);
			zval_ptr_dtor(&code);
			efree(func_params);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "call to error callback failed for request: %s", Z_STRVAL_PP(method));
			return;
		}
	} zend_catch {
		bailout = 1;
	} zend_end_try();

	zval_ptr_dtor(&err);
	zval_ptr_dtor(&code);
	if (retval_ptr) {
		zval_ptr_dtor(&retval_ptr);
	}

	efree(func_params);

	return bailout? 0 : 1;
} /* }}} */

int php_yar_concurrent_client_handle(zval *client, zval *callstack, zval *callback, zval *error_callback TSRMLS_DC) /* {{{ */ {
	zval **entry;
	char *dummy;
	long sequence;
	zval **uri, **method, **parameters;
	yar_transport_t *factory;
	yar_transport_interface_t *transport;
	yar_transport_multi_interface_t *multi;

	factory = php_yar_transport_get(ZEND_STRL("curl") TSRMLS_CC);
	multi = factory->multi->init(TSRMLS_C);

	for(zend_hash_internal_pointer_reset(Z_ARRVAL_P(callstack));
			zend_hash_has_more_elements(Z_ARRVAL_P(callstack)) == SUCCESS;
			zend_hash_move_forward(Z_ARRVAL_P(callstack))) {
		if (zend_hash_get_current_data(Z_ARRVAL_P(callstack), (void**)&entry) == FAILURE) {
			continue;
		}
		if (Z_TYPE_PP(entry) != IS_ARRAY) {
			php_yar_client_trigger_error(1 TSRMLS_CC, 0, "unexpect non-array call entry, internal error");
			multi->close(multi TSRMLS_CC);
			return 0;
		}
		zend_hash_get_current_key(Z_ARRVAL_P(callstack), &dummy, &sequence, 0);

		zend_hash_find(Z_ARRVAL_PP(entry), ZEND_STRS("u"), (void **)&uri);
		zend_hash_find(Z_ARRVAL_PP(entry), ZEND_STRS("m"), (void **)&method);

		if (zend_hash_find(Z_ARRVAL_PP(entry), ZEND_STRS("p"), (void **)&parameters) == FAILURE) {
			zval *tmp;
			MAKE_STD_ZVAL(tmp);
			array_init(tmp);
			parameters = &tmp;
		}
		transport = factory->init(TSRMLS_C);
		if (!php_yar_client_http_prepare(transport, Z_STRVAL_PP(uri), Z_STRLEN_PP(uri),
					Z_STRVAL_PP(method), Z_STRLEN_PP(method), *parameters TSRMLS_CC)) {
			transport->close(transport TSRMLS_CC);
			multi->close(multi TSRMLS_CC);
			return 0;
		}
		add_assoc_long_ex(*entry, ZEND_STRS("i"), sequence);
		transport->calldata(transport, *entry TSRMLS_CC);
		multi->add(multi, transport TSRMLS_CC);
	}

	if (!multi->exec(multi, php_yar_concurrent_client_callback,
				error_callback? php_yar_concurrent_client_error_callback : NULL, callback, error_callback TSRMLS_CC)) {
		multi->close(multi TSRMLS_CC);
		return 0;
	}

	multi->close(multi TSRMLS_CC);
	return 1;
} /* }}} */

/* {{{ proto Yar_Client::__construct($uri, array $options = NULL) */
PHP_METHOD(yar_client, __construct) {
	char *url;
	long len;
	zval *options = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a!", &url, &len, &options) == FAILURE) {
        return;
    }

    zend_update_property_stringl(yar_client_ce, getThis(), ZEND_STRL("_uri"), url, len TSRMLS_CC);

	if (options) {
    	zend_update_property(yar_client_ce, getThis(), ZEND_STRL("_options"), options TSRMLS_CC);
	}
}
/* }}} */

/* {{{ proto Yar_Client::__call($method, $parameters = NULL) */
PHP_METHOD(yar_client, __call) {
	zval *params, *protocol, *ret;
	char *method;
	long mlen;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &method, &mlen, &params) == FAILURE) {
		return;
	}

	protocol = zend_read_property(yar_client_ce, getThis(), ZEND_STRL("_protocol"), 0 TSRMLS_CC);

	/* we only support HTTP now */
	switch (Z_LVAL_P(protocol)) {
		default:
			if ((ret = php_yar_client_http_handle(getThis(), method, mlen, params TSRMLS_CC))) {
				RETVAL_ZVAL(ret, 0, 0);
				efree(ret);
				return;
			}
			break;
	}

	RETURN_NULL();
}
/* }}} */

/* {{{ proto Yar_Client::call($method, $parameters = NULL) */
PHP_METHOD(yar_client, call) {
	PHP_MN(yar_client___call)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ proto Yar_Concurrent_Client::call($uri, $method, $parameters = NULL, $callback = NULL) */
PHP_METHOD(yar_concurrent_client, call) {
	char *uri, *method, *name;
	long ulen, mlen, sequence;
	zval *callback = NULL, *parameters = NULL;
	zval *callstack, *item;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a!z",
				&uri, &ulen, &method, &mlen, &parameters, &callback) == FAILURE) {
		return;
	}

    if (callback && !ZVAL_IS_NULL(callback) && !zend_is_callable(callback, 0, &name TSRMLS_CC)) {
        php_error_docref1(NULL TSRMLS_CC, name, E_ERROR, "third parameter is expected to be a valid callback");
        efree(name);
        RETURN_FALSE;
    }

	MAKE_STD_ZVAL(item);
	array_init(item);

	add_assoc_stringl_ex(item, ZEND_STRS("u"), uri, ulen, 1);
	add_assoc_stringl_ex(item, ZEND_STRS("m"), method, mlen, 1);
	if (callback && !ZVAL_IS_NULL(callback)) {
		Z_ADDREF_P(callback);
		add_assoc_zval_ex(item, ZEND_STRS("c"), callback);
	}
	if (parameters && IS_ARRAY == Z_TYPE_P(parameters)) {
		Z_ADDREF_P(parameters);
		add_assoc_zval_ex(item, ZEND_STRS("p"), parameters);
	}

	callstack = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callstack"), 0 TSRMLS_CC);
	if (ZVAL_IS_NULL(callstack)) {
		array_init(callstack);
	}
	sequence = zend_hash_next_free_element(Z_ARRVAL_P(callstack));
	add_next_index_zval(callstack, item);

	RETURN_LONG(sequence);
}
/* }}} */

/* {{{ proto Yar_Concurrent_Client::loop($callback = NULL, $error_callbck = NULL) */
PHP_METHOD(yar_concurrent_client, loop) {
	char *name = NULL;
	zval *callback = NULL, *error_callback = NULL;
	zval *callstack, *item, *sequence;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zz", &callback, &error_callback) == FAILURE) {
		return;
	}

    if (callback && !ZVAL_IS_NULL(callback) && !zend_is_callable(callback, 0, &name TSRMLS_CC)) {
        php_error_docref1(NULL TSRMLS_CC, name, E_ERROR, "the first argument is expected to be a valid callback");
        efree(name);
        RETURN_FALSE;
    }

	if (name) {
		efree(name);
		name = NULL;
	}

    if (error_callback && !ZVAL_IS_NULL(error_callback) && !zend_is_callable(error_callback, 0, &name TSRMLS_CC)) {
        php_error_docref1(NULL TSRMLS_CC, name, E_ERROR, "the second argument is expected to be a valid callback");
        efree(name);
        RETURN_FALSE;
    }

	if (name) {
		efree(name);
	}

	if (callback && ZVAL_IS_NULL(callback)) {
		callback = NULL;
	}

	if (error_callback && ZVAL_IS_NULL(error_callback)) {
		error_callback = NULL;
	}


	callstack = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callstack"), 0 TSRMLS_CC);
	if (ZVAL_IS_NULL(callstack) || zend_hash_num_elements(Z_ARRVAL_P(callstack)) == 0) {
		RETURN_TRUE;
	}

    RETURN_BOOL(php_yar_concurrent_client_handle(getThis(), callstack, callback, error_callback TSRMLS_CC));
}
/* }}} */

/* {{{ yar_client_methods */
zend_function_entry yar_client_methods[] = {
	PHP_ME(yar_client, __construct, arginfo_client___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)
	PHP_ME(yar_client, call, arginfo_client___call, ZEND_ACC_PUBLIC)
	PHP_ME(yar_client, __call, arginfo_client___call, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

/* {{{ yar_concurrent_client_methods */
zend_function_entry yar_concurrent_client_methods[] = {
	PHP_ME(yar_concurrent_client, call, arginfo_client_async, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(yar_concurrent_client, loop, arginfo_client_loop, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_FE_END
};
/* }}} */

YAR_STARTUP_FUNCTION(client) /* {{{ */ {
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "Yar_Client", yar_client_methods);
    yar_client_ce = zend_register_internal_class(&ce TSRMLS_CC);

	zend_declare_property_long(yar_client_ce, ZEND_STRL("_protocol"), YAR_CLIENT_PROTOCOL_HTTP, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yar_client_ce, ZEND_STRL("_uri"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yar_client_ce, ZEND_STRL("_options"),  ZEND_ACC_PROTECTED|ZEND_ACC_STATIC TSRMLS_CC);

	INIT_CLASS_ENTRY(ce, "Yar_Concurrent_Client", yar_concurrent_client_methods);
    yar_concurrent_client_ce = zend_register_internal_class(&ce TSRMLS_CC);	
	zend_declare_property_null(yar_concurrent_client_ce, ZEND_STRL("_callstack"), ZEND_ACC_PROTECTED|ZEND_ACC_STATIC TSRMLS_CC);

	REGISTER_LONG_CONSTANT("YAR_CLIENT_PROTOCOL_HTTP", YAR_CLIENT_PROTOCOL_HTTP, CONST_PERSISTENT | CONST_CS);

    return SUCCESS;
}
/* }}} */

YAR_SHUTDOWN_FUNCTION(client) /* {{{ */ {
	return SUCCESS;
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
