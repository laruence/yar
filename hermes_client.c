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
#include "SAPI.h"
#include "php_hermes.h"
#include "hermes_exception.h"
#include "hermes_packager.h"
#include "hermes_transport.h"
#include "hermes_client.h"
#include "hermes_request.h"
#include "hermes_response.h"
#include "hermes_protocol.h"
#include "ext/standard/php_rand.h" /* for php_mt_rand */
#include "ext/standard/url.h" /* for php_url */

zend_class_entry *hermes_client_ce;

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
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, parameters)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(arginfo_client_loop, 0, 0, 0)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()
/* }}} */

static zval * php_hermes_client_parse_response(char *ret, size_t len, int throw_exception TSRMLS_DC) /* {{{ */ {
	zval *retval, *response;
	hermes_header_t *header;
	char *err_msg;

	MAKE_STD_ZVAL(retval);
	ZVAL_FALSE(retval);

	if (!(header = php_hermes_protocol_parse(&ret, &len, &err_msg TSRMLS_CC))) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", err_msg);
	    php_hermes_debug_client("0: malformed response '%s'" TSRMLS_CC, ret);
		efree(err_msg);
		return retval;
	}

	if (!len || !header->body_len) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "server response empty body");
		return retval;
	}

	php_hermes_debug_client("%ld: server responsed: packager '%s', len '%ld', content '%s'" TSRMLS_CC, header->id, ret, len - 8, ret + 8);

	if (!(response = php_hermes_packager_unpack(ret, len, &err_msg TSRMLS_CC))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", err_msg);
		efree(err_msg);
		return retval;
	}

	if (response && IS_ARRAY == Z_TYPE_P(response)) {
		zval **ppzval;
		HashTable *ht = Z_ARRVAL_P(response);
	    if (zend_hash_find(ht, ZEND_STRS("s"), (void **)&ppzval) == FAILURE) {
		}

	    convert_to_long(*ppzval);

		if (Z_LVAL_PP(ppzval) == HERMES_ERR_OKEY) {
			if (zend_hash_find(ht, ZEND_STRS("o"), (void **)&ppzval) == SUCCESS) {
				PHPWRITE(Z_STRVAL_PP(ppzval), Z_STRLEN_PP(ppzval));
			}
		} else if (throw_exception && Z_LVAL_PP(ppzval) == HERMES_ERR_EXCEPTION) {
			if (zend_hash_find(ht, ZEND_STRS("e"), (void **)&ppzval) == SUCCESS) {
				zval *ex, **property;
				MAKE_STD_ZVAL(ex);
				object_init_ex(ex, hermes_exception_server_ce);

				if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("message"), (void **)&property) == SUCCESS) {
					zend_update_property(hermes_exception_server_ce, ex, ZEND_STRL("message"), *property TSRMLS_CC);
				}

				if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("code"), (void **)&property) == SUCCESS) {
					zend_update_property(hermes_exception_server_ce, ex, ZEND_STRL("code"), *property TSRMLS_CC);
				}

				if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("file"), (void **)&property) == SUCCESS) {
					zend_update_property(hermes_exception_server_ce, ex, ZEND_STRL("file"), *property TSRMLS_CC);
				}

				if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("line"), (void **)&property) == SUCCESS) {
					zend_update_property(hermes_exception_server_ce, ex, ZEND_STRL("line"), *property TSRMLS_CC);
				}

				if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("_type"), (void **)&property) == SUCCESS) {
					zend_update_property(hermes_exception_server_ce, ex, ZEND_STRL("_type"), *property TSRMLS_CC);
				}

				zend_throw_exception_internal(ex TSRMLS_CC);
			}
		} else if (zend_hash_find(ht, ZEND_STRS("e"), (void **)&ppzval) == SUCCESS
				&& IS_STRING == Z_TYPE_PP(ppzval)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", Z_STRVAL_PP(ppzval));
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

static int php_hermes_client_http_prepare(hermes_transport_interface_t *transport, char *uri, long ulen, char *method, long mlen, zval *params) /* {{{ */ {
	char *payload, *err_msg;
	size_t payload_len;
	zval request;
	hermes_header_t header = {0};
	long request_id;
	php_url *url;
   
	if (!BG(mt_rand_is_seeded)) {
		php_mt_srand(GENERATE_SEED() TSRMLS_CC);
	}
    request_id = (long)php_mt_rand(TSRMLS_C);

	INIT_ZVAL(request);
	array_init(&request);
	zend_hash_init(Z_ARRVAL(request), 4, NULL, NULL, 0);

	add_assoc_long_ex(&request, ZEND_STRS("i"), request_id);
	add_assoc_stringl_ex(&request, ZEND_STRS("m"), method, mlen, 0);
    add_assoc_zval_ex(&request, ZEND_STRS("p"), params);

	php_hermes_debug_client("%ld: call api '%s' at '%s' with '%d parameters'" TSRMLS_CC,
			request_id, method, uri, zend_hash_num_elements(Z_ARRVAL_P(params)));

	if (!(payload_len = php_hermes_packager_pack(&request, &payload, &err_msg TSRMLS_CC))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", err_msg);
		efree(err_msg);
		return 0;
	}
	php_hermes_debug_client("%ld: package result: packager '%s', len: '%ld', content '%s'" TSRMLS_CC,
			request_id, payload, payload_len - 8, payload + 8);

	if (!(url = php_url_parse(uri))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "malformed uri: '%s'", uri);
		return 0;
	}

	transport->open(transport, uri, ulen, url->host, 0 TSRMLS_CC);

	php_hermes_protocol_render(&header, request_id, url->user, url->pass, payload_len, 0 TSRMLS_CC);

	php_url_free(url);

    transport->send(transport, (char *)&header, sizeof(hermes_header_t) TSRMLS_CC);
    transport->send(transport, payload, payload_len TSRMLS_CC);

	return 1;
} /* }}} */

static zval * php_hermes_client_http_handle(zval *client, char *method, long mlen, zval *params TSRMLS_DC) /* {{{ */ {
	zval *uri;
	hermes_transport_t *factory;
	hermes_transport_interface_t *transport;
	char *ret;
	size_t ret_len;

	uri = zend_read_property(hermes_client_ce, client, ZEND_STRL("_uri"), 0 TSRMLS_CC);

	factory = php_hermes_transport_get(ZEND_STRL("curl") TSRMLS_CC);
	transport = factory->init(TSRMLS_C);

 	if (!php_hermes_client_http_prepare(transport, Z_STRVAL_P(uri), Z_STRLEN_P(uri), method, mlen, params TSRMLS_CC)) {
		transport->close(transport TSRMLS_CC);
		factory->destroy(transport TSRMLS_CC);
        return NULL;
	}

	if (transport->exec(transport, &ret, &ret_len TSRMLS_CC)) {
		zval *response = NULL;
		if (ret_len) {
			response = php_hermes_client_parse_response(ret, ret_len, 1 TSRMLS_CC);
			efree(ret);
		} else {
		    php_error_docref(NULL TSRMLS_DC, E_WARNING, "server responsed empty content");
		}
		transport->close(transport TSRMLS_CC);
		factory->destroy(transport TSRMLS_CC);
		return response;
	}

	transport->close(transport TSRMLS_CC);
	factory->destroy(transport TSRMLS_CC);

	return NULL;
} /* }}} */

void php_hermes_client_async_callback(zval *calldata, void *gcallback, char *ret, size_t len TSRMLS_DC) /* {{{ */ {
	zval **callback, **method, **uri, **sequence;
	zval *response = NULL, *retval_ptr = NULL;
	zval ***func_params;
	zval *callback1 = (zval *)gcallback;

	if (len) {
		response = php_hermes_client_parse_response(ret, len, 0 TSRMLS_CC);
		efree(ret);
	} else {
		php_error_docref(NULL TSRMLS_DC, E_WARNING, "server responsed empty content");
		return;
	}

	zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("u"), (void **)&uri);
	zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("m"), (void **)&method);
	if (zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("c"), (void **)&callback) == FAILURE) {
		callback = &callback1;
	}
	zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("i"), (void **)&sequence);

	func_params = emalloc(sizeof(zval **) * 4);
	func_params[0] = &response;
	func_params[1] = sequence;
	func_params[2] = method;
	func_params[3] = uri;

	if (call_user_function_ex(EG(function_table), NULL, *callback, &retval_ptr, 4, func_params, 0, NULL TSRMLS_CC) != SUCCESS) {
		efree(func_params);
		php_error_docref(NULL TSRMLS_DC, E_WARNING, "call to callback failed for request: %s", Z_STRVAL_PP(method));
		return;
	}

	if (retval_ptr) {
		zval_ptr_dtor(&retval_ptr);
	}

	efree(func_params);
} /* }}} */

int php_hermes_client_async_handle(zval *client, zval *callstack, zval *callback TSRMLS_DC) /* {{{ */ {
	zval **entry;
	char *dummy;
	long sequence;
	zval **uri, **method, **parameters;
	hermes_transport_t *factory;
	hermes_transport_interface_t *transport;
	hermes_transport_multi_interface_t *multi;

	factory = php_hermes_transport_get(ZEND_STRL("curl") TSRMLS_CC);
	multi = factory->multi->init(TSRMLS_C);

	for(zend_hash_internal_pointer_reset(Z_ARRVAL_P(callstack));
			zend_hash_has_more_elements(Z_ARRVAL_P(callstack)) == SUCCESS;
			zend_hash_move_forward(Z_ARRVAL_P(callstack))) {
		if (zend_hash_get_current_data(Z_ARRVAL_P(callstack), (void**)&entry) == FAILURE) {
			continue;
		}
		if (Z_TYPE_PP(entry) != IS_ARRAY) {
			php_error_docref(NULL TSRMLS_DC, E_ERROR, "unexpect non-array call entry, internal error");
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
		if (!php_hermes_client_http_prepare(transport, Z_STRVAL_PP(uri), Z_STRLEN_PP(uri),
					Z_STRVAL_PP(method), Z_STRLEN_PP(method), *parameters TSRMLS_CC)) {
			break;
		}
		add_assoc_long_ex(*entry, ZEND_STRS("i"), sequence);
		transport->calldata(transport, *entry TSRMLS_CC);
		multi->add(multi, transport TSRMLS_CC);
	}

	multi->exec(multi, php_hermes_client_async_callback, callback TSRMLS_CC);
	multi->close(multi);

} /* }}} */

/* {{{ proto Hermes_Client::__construct($uri, array $options = NULL) */
PHP_METHOD(hermes_client, __construct) {
	char *url;
	long len;
	zval *options = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a!", &url, &len, &options) == FAILURE) {
        return;
    }

    zend_update_property_stringl(hermes_client_ce, getThis(), ZEND_STRL("_uri"), url, len TSRMLS_CC);

	if (options) {
    	zend_update_property(hermes_client_ce, getThis(), ZEND_STRL("_options"), options TSRMLS_CC);
	}
}
/* }}} */

/* {{{ proto Hermes_Client::__call($method, $parameters = NULL) */
PHP_METHOD(hermes_client, __call) {
	zval *params, *protocol, *ret;
	char *method;
	long mlen;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &method, &mlen, &params) == FAILURE) {
		return;
	}

	protocol = zend_read_property(hermes_client_ce, getThis(), ZEND_STRL("_protocol"), 0 TSRMLS_CC);

	/* we only support HTTP now */
	switch (Z_LVAL_P(protocol)) {
		default:
			if ((ret = php_hermes_client_http_handle(getThis(), method, mlen, params TSRMLS_CC))) {
				RETURN_ZVAL(ret, 0, 0);
			}
			break;
	}

	RETURN_NULL();
}
/* }}} */

/* {{{ proto Hermes_Client::call($method, $parameters = NULL) */
PHP_METHOD(hermes_client, call) {
	PHP_MN(hermes_client___call)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ proto Hermes_Client::async($uri, $method, $callback = NULL, $parameters = NULL) */
PHP_METHOD(hermes_client, async) {
	char *uri, *method, *name;
	long ulen, mlen, sequence;
	zval *callback = NULL, *parameters = NULL;
	zval *callstack, *item;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|za",
				&uri, &ulen, &method, &mlen, &callback, &parameters) == FAILURE) {
		return;
	}

    if (callback && !ZVAL_IS_NULL(callback) && !zend_is_callable(callback, 0, &name)) {
        php_error_docref1(NULL TSRMLS_CC, name, E_WARNING, "third parameter is expected to be a valid callback");
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
	if (parameters) {
		Z_ADDREF_P(parameters);
		add_assoc_zval_ex(item, ZEND_STRS("p"), parameters);
	}

	callstack = zend_read_static_property(hermes_client_ce, ZEND_STRL("_callstack"), 0 TSRMLS_CC);
	if (ZVAL_IS_NULL(callstack)) {
		array_init(callstack);
	}
	sequence = zend_hash_next_free_element(Z_ARRVAL_P(callstack));
	add_next_index_zval(callstack, item);

	RETURN_LONG(sequence);
}
/* }}} */

/* {{{ proto Hermes_Client::loop($callback = NULL) */
PHP_METHOD(hermes_client, loop) {
	char *name;
	zval *callback = NULL;
	zval *callstack, *item, *sequence;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &callback) == FAILURE) {
		return;
	}

    if (callback && !zend_is_callable(callback, 0, &name)) {
        php_error_docref1(NULL TSRMLS_CC, name, E_WARNING, "argument is expected to be a valid callback");
        efree(name);
        RETURN_FALSE;
    }

	callstack = zend_read_static_property(hermes_client_ce, ZEND_STRL("_callstack"), 0 TSRMLS_CC);
	if (ZVAL_IS_NULL(callstack) || zend_hash_num_elements(Z_ARRVAL_P(callstack)) == 0) {
		RETURN_TRUE;
	}

    RETURN_BOOL(php_hermes_client_async_handle(getThis(), callstack, callback TSRMLS_CC));
}
/* }}} */

/* {{{ hermes_client_methods */
zend_function_entry hermes_client_methods[] = {
	PHP_ME(hermes_client, __construct, arginfo_client___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)
	PHP_ME(hermes_client, call, arginfo_client___call, ZEND_ACC_PUBLIC)
	PHP_ME(hermes_client, __call, arginfo_client___call, ZEND_ACC_PUBLIC)
	PHP_ME(hermes_client, async, arginfo_client_async, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(hermes_client, loop, arginfo_client_loop, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_FE_END
};
/* }}} */

HERMES_STARTUP_FUNCTION(client) /* {{{ */ {
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "Hermes_Client", hermes_client_methods);
    hermes_client_ce = zend_register_internal_class(&ce TSRMLS_CC);

	zend_declare_property_long(hermes_client_ce, ZEND_STRL("_protocol"), HERMES_CLIENT_PROTOCOL_HTTP, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(hermes_client_ce, ZEND_STRL("_uri"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(hermes_client_ce, ZEND_STRL("_options"),  ZEND_ACC_PROTECTED|ZEND_ACC_STATIC TSRMLS_CC);
	zend_declare_property_null(hermes_client_ce, ZEND_STRL("_callstack"), ZEND_ACC_PROTECTED|ZEND_ACC_STATIC TSRMLS_CC);

	REGISTER_LONG_CONSTANT("HERMES_CLIENT_PROTOCOL_HTTP", HERMES_CLIENT_PROTOCOL_HTTP, CONST_PERSISTENT | CONST_CS);

    return SUCCESS;
}
/* }}} */

HERMES_SHUTDOWN_FUNCTION(client) /* {{{ */ {
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
