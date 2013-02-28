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
#include "SAPI.h"
#include "Zend/zend_exceptions.h"
#include "ext/standard/url.h" /* for php_url */

#include "php_yar.h"
#include "yar_exception.h"
#include "yar_packager.h"
#include "yar_transport.h"
#include "yar_client.h"
#include "yar_request.h"
#include "yar_response.h"
#include "yar_protocol.h"

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
ZEND_BEGIN_ARG_INFO_EX(arginfo_client_getopt, 0, 0, 2)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(arginfo_client_setopt, 0, 0, 2)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(arginfo_client_async, 0, 0, 3)
	ZEND_ARG_INFO(0, uri)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, parameters)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(arginfo_client_loop, 0, 0, 0)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, error_callback)
ZEND_END_ARG_INFO()
/* }}} */

static void php_yar_client_trigger_error(int throw_exception TSRMLS_DC, int code, const char *format, ...) /* {{{ */ {
	va_list arg;
	char *message;
	zend_class_entry *ce;

	va_start(arg, format);
	vspprintf(&message, 0, format, arg);
	va_end(arg);

	if (throw_exception) {
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
			case YAR_ERR_REQUEST:
			case YAR_ERR_EXCEPTION:
				ce = yar_server_exception_ce;
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

static void php_yar_client_handle_error(int throw_exception, yar_response_t *response TSRMLS_DC) /* {{{ */ {
	if (response->status == YAR_ERR_EXCEPTION) {
		if (throw_exception) {
			zval *ex, **property;
			MAKE_STD_ZVAL(ex);
			object_init_ex(ex, yar_server_exception_ce);

			if (zend_hash_find(Z_ARRVAL_P(response->err), ZEND_STRS("message"), (void **)&property) == SUCCESS) {
				zend_update_property(yar_server_exception_ce, ex, ZEND_STRL("message"), *property TSRMLS_CC);
			}

			if (zend_hash_find(Z_ARRVAL_P(response->err), ZEND_STRS("code"), (void **)&property) == SUCCESS) {
				zend_update_property(yar_server_exception_ce, ex, ZEND_STRL("code"), *property TSRMLS_CC);
			}

			if (zend_hash_find(Z_ARRVAL_P(response->err), ZEND_STRS("file"), (void **)&property) == SUCCESS) {
				zend_update_property(yar_server_exception_ce, ex, ZEND_STRL("file"), *property TSRMLS_CC);
			}

			if (zend_hash_find(Z_ARRVAL_P(response->err), ZEND_STRS("line"), (void **)&property) == SUCCESS) {
				zend_update_property(yar_server_exception_ce, ex, ZEND_STRL("line"), *property TSRMLS_CC);
			}

			if (zend_hash_find(Z_ARRVAL_P(response->err), ZEND_STRS("_type"), (void **)&property) == SUCCESS) {
				zend_update_property(yar_server_exception_ce, ex, ZEND_STRL("_type"), *property TSRMLS_CC);
			}
			zend_throw_exception_object(ex TSRMLS_CC);
		} else {
			zval **msg, **code;
			if (zend_hash_find(Z_ARRVAL_P(response->err), ZEND_STRS("message"), (void **)&msg) == SUCCESS
					&& zend_hash_find(Z_ARRVAL_P(response->err), ZEND_STRS("code"), (void **)&code) == SUCCESS) {
				convert_to_string_ex(msg);
				convert_to_long_ex(code);
				php_yar_client_trigger_error(0 TSRMLS_CC, Z_LVAL_PP(code), "server threw an exception with message `%s`", Z_STRVAL_PP(msg));
			}
		}
	} else {
		php_yar_client_trigger_error(throw_exception TSRMLS_CC, response->status, "%s", Z_STRVAL_P(response->err));
	}
}
/* }}} */

static zval * php_yar_client_get_opt(zval *options, long type TSRMLS_DC) /* {{{ */ {
	zval **value;
	if (IS_ARRAY != Z_TYPE_P(options)) {
		return NULL;
	}

	if (zend_hash_index_find(Z_ARRVAL_P(options), type, (void **)&value) == SUCCESS) {
		return *value;
	}

	return NULL;
} /* }}} */

static int php_yar_client_set_opt(zval *client, long type, zval *value TSRMLS_DC) /* {{{ */ {
	zend_bool verified = 0;

	switch (type) {
		case YAR_OPT_PACKAGER:
		{
			 verified = 1;
             if (IS_STRING != Z_TYPE_P(value)) {
				 php_error_docref(NULL TSRMLS_CC, E_WARNING, "expects a string packager name");
				 return 0;
			 }
		}
		case YAR_OPT_PERSISTENT:
		{
			if (!verified) {
				verified = 1;
				if (IS_LONG != Z_TYPE_P(value) && IS_BOOL != Z_TYPE_P(value)) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "expects a boolean persistent flag");
					return 0;
				}

			}
		}
		case YAR_OPT_TIMEOUT:
		case YAR_OPT_CONNECT_TIMEOUT:
		{
			zval *options;

			if (!verified) {
				if (IS_LONG != Z_TYPE_P(value)) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "expects a long integer timeout value");
					return 0;
				}
			}

			options = zend_read_property(yar_client_ce, client, ZEND_STRL("_options"), 0 TSRMLS_CC);
			if (IS_ARRAY != Z_TYPE_P(options)) {
				MAKE_STD_ZVAL(options);
				array_init(options);
				zend_update_property(yar_client_ce, client, ZEND_STRL("_options"), options TSRMLS_CC);
				zval_ptr_dtor(&options);
			}

			Z_ADDREF_P(value);
			zend_hash_index_update(Z_ARRVAL_P(options), type, (void **)&value, sizeof(zval *), NULL);
			break;
		}
		default:
			return 0;
	}

	return 1;
} /* }}} */

static zval * php_yar_client_handle(int protocol, zval *client, char *method, long mlen, zval *params TSRMLS_DC) /* {{{ */ {
	char *msg;
	zval *uri, *options, *retval;
	yar_transport_t *factory;
	yar_transport_interface_t *transport;
	yar_request_t *request;
	yar_response_t *response;
	int flags = 0;

	uri = zend_read_property(yar_client_ce, client, ZEND_STRL("_uri"), 0 TSRMLS_CC);

	if (protocol == YAR_CLIENT_PROTOCOL_HTTP) {
		factory = php_yar_transport_get(ZEND_STRL("curl") TSRMLS_CC);
	} else if (protocol == YAR_CLIENT_PROTOCOL_TCP || protocol == YAR_CLIENT_PROTOCOL_UNIX) {
		factory = php_yar_transport_get(ZEND_STRL("sock") TSRMLS_CC);
	}

	transport = factory->init(TSRMLS_C);

	options = zend_read_property(yar_client_ce, client, ZEND_STRL("_options"), 1 TSRMLS_CC);

	if (IS_ARRAY != Z_TYPE_P(options)) {
		options = NULL;
	}

	if (!(request = php_yar_request_instance(method, mlen, params, options TSRMLS_CC))) {
		transport->close(transport TSRMLS_CC);
		factory->destroy(transport TSRMLS_CC);
		return NULL;
	}

	if (YAR_G(allow_persistent)) {
		if (options) {
			zval *flag = php_yar_client_get_opt(options, YAR_OPT_PERSISTENT TSRMLS_CC);
			if (flag && (Z_TYPE_P(flag) == IS_BOOL || Z_TYPE_P(flag) == IS_LONG) && Z_LVAL_P(flag)) {
				flags |= YAR_PROTOCOL_PERSISTENT;
			}
		}
	}

	if (!transport->open(transport, Z_STRVAL_P(uri), Z_STRLEN_P(uri), flags, &msg TSRMLS_CC)) {
		php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_TRANSPORT, msg TSRMLS_CC);
		php_yar_request_destroy(request TSRMLS_CC);
		efree(msg);
		return NULL;
	}

	DEBUG_C("%ld: call api '%s' at (%c)'%s' with '%d' parameters",
			request->id, request->method, (flags & YAR_PROTOCOL_PERSISTENT)? 'p' : 'r', Z_STRVAL_P(uri), zend_hash_num_elements(Z_ARRVAL_P(request->parameters)));

	if (!transport->send(transport, request, &msg TSRMLS_CC)) {
		php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_TRANSPORT, msg TSRMLS_CC);
		php_yar_request_destroy(request TSRMLS_CC);
		efree(msg);
		return NULL;
	}

	response = transport->exec(transport, request TSRMLS_CC);

	if (response->status != YAR_ERR_OKEY) {
		php_yar_client_handle_error(1, response TSRMLS_CC);
		retval = NULL;
	} else {
		if (response->olen) {
			PHPWRITE(response->out, response->olen);
		}
		if ((retval = response->retval)) {
			Z_ADDREF_P(retval);
		}
	}

	php_yar_request_destroy(request TSRMLS_CC);
	php_yar_response_destroy(response TSRMLS_CC);
	transport->close(transport TSRMLS_CC);
	factory->destroy(transport TSRMLS_CC);

	return retval;
} /* }}} */

int php_yar_concurrent_client_callback(yar_call_data_t *calldata, int status, yar_response_t *response TSRMLS_DC) /* {{{ */ {
	zval *code, *retval, *retval_ptr = NULL;
	zval *callinfo, *callback, ***func_params;
	zend_bool bailout = 0;
	uint params_count;

	if (calldata) {
		/* data callback */
		if (status == YAR_ERR_OKEY) {
			if (calldata->callback) {
				callback = calldata->callback;
			} else {
				callback = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callback"), 0 TSRMLS_CC);
			}
			params_count = 2;
		} else {
			if (calldata->ecallback) {
				callback = calldata->ecallback;
			} else {
				callback = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_error_callback"), 0 TSRMLS_CC);
			}
			params_count = 3;
		}

		if (ZVAL_IS_NULL(callback)) {
			if (status != YAR_ERR_OKEY) {
				if (response->err) {
					php_yar_client_handle_error(0, response TSRMLS_CC);
				} else {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "[%d]:unknown Error", status);
				}
			} else if (response->retval) {
				zend_print_zval(response->retval, 1);
			}
			return 1;
		}

		if (status == YAR_ERR_OKEY) {
			if (!response->retval) {
				php_yar_client_trigger_error(0 TSRMLS_CC, YAR_ERR_REQUEST, "%s", "server responsed empty response");
				return 1;
			}
			Z_ADDREF_P(response->retval);
			retval = response->retval;
		} else {
			MAKE_STD_ZVAL(code);
			ZVAL_LONG(code, status);
			Z_ADDREF_P(response->err);
			retval = response->err;
		}

		MAKE_STD_ZVAL(callinfo);
		array_init(callinfo);

		add_assoc_long_ex(callinfo, "sequence", sizeof("sequence"), calldata->sequence);
		add_assoc_stringl_ex(callinfo, "uri", sizeof("uri"), calldata->uri, calldata->ulen, 1);
		add_assoc_stringl_ex(callinfo, "method", sizeof("method"), calldata->method, calldata->mlen, 1);
	} else {
		callback = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callback"), 0 TSRMLS_CC);
		if (ZVAL_IS_NULL(callback)) {
			return 1;
		}
		params_count = 2;
	}

	func_params = emalloc(sizeof(zval **) * params_count);
	if (calldata && (status != YAR_ERR_OKEY)) {
		func_params[0] = &code;
		func_params[1] = &retval;
		func_params[2] = &callinfo;
	} else if (calldata) {
		func_params[0] = &retval;
		func_params[1] = &callinfo;
	} else {
		MAKE_STD_ZVAL(retval);
		MAKE_STD_ZVAL(callinfo);
		ZVAL_NULL(retval);
		ZVAL_NULL(callinfo);
		func_params[0] = &retval;
		func_params[1] = &callinfo;
	}

	zend_try {
		if (call_user_function_ex(EG(function_table), NULL, callback,
					&retval_ptr, params_count, func_params, 0, NULL TSRMLS_CC) != SUCCESS) {
			if (status) {
				zval_ptr_dtor(&code);
			}
			zval_ptr_dtor(&retval);
			zval_ptr_dtor(&callinfo);
			efree(func_params);
			if (calldata) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "call to callback failed for request: '%s'", calldata->method);
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "call to initial callback failed");
			}
			return 1;
		}
	} zend_catch {
		bailout = 1;
	} zend_end_try();

	zval_ptr_dtor(&retval);
	zval_ptr_dtor(&callinfo);
	if (status) {
		zval_ptr_dtor(&code);
	}

	if (retval_ptr) {
		zval_ptr_dtor(&retval_ptr);
	}

	efree(func_params);
    return bailout? 0 : 1;
} /* }}} */

int php_yar_concurrent_client_handle(zval *callstack TSRMLS_DC) /* {{{ */ {
	char *dummy, *msg;
	ulong sequence;
	zval **calldata;
	yar_request_t *request;
	yar_transport_t *factory;
	yar_transport_interface_t *transport;
	yar_transport_multi_interface_t *multi;

	factory = php_yar_transport_get(ZEND_STRL("curl") TSRMLS_CC);
	multi = factory->multi->init(TSRMLS_C);

	for(zend_hash_internal_pointer_reset(Z_ARRVAL_P(callstack));
			zend_hash_has_more_elements(Z_ARRVAL_P(callstack)) == SUCCESS;
			zend_hash_move_forward(Z_ARRVAL_P(callstack))) {
		yar_call_data_t *entry;
		long flags = 0;

		if (zend_hash_get_current_data(Z_ARRVAL_P(callstack), (void**)&calldata) == FAILURE) {
			continue;
		}

		ZEND_FETCH_RESOURCE_NO_RETURN(entry, yar_call_data_t *, calldata, -1, "Yar Call Data", le_calldata);

		if (!entry) {
			continue;
		}

		zend_hash_get_current_key(Z_ARRVAL_P(callstack), &dummy, &sequence, 0);

		if (!entry->parameters) {
			zval *tmp;
			MAKE_STD_ZVAL(tmp);
			array_init(tmp);
			entry->parameters = tmp;
		} 

		transport = factory->init(TSRMLS_C);

		if (YAR_G(allow_persistent)) {
			if (entry->options) {
				zval *flag = php_yar_client_get_opt(entry->options, YAR_OPT_PERSISTENT TSRMLS_CC);
				if (flag && (Z_TYPE_P(flag) == IS_BOOL || Z_TYPE_P(flag) == IS_LONG) && Z_LVAL_P(flag)) {
					flags |= YAR_PROTOCOL_PERSISTENT;
				}
			}
		}

		if (!(request = php_yar_request_instance(entry->method, entry->mlen, entry->parameters, entry->options TSRMLS_CC))) {
			transport->close(transport TSRMLS_CC);
			factory->destroy(transport TSRMLS_CC);
			return 0;
		}

		if (!transport->open(transport, entry->uri, entry->ulen, flags, &msg TSRMLS_CC)) {
			php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_TRANSPORT, msg TSRMLS_CC);
			transport->close(transport TSRMLS_CC);
			factory->destroy(transport TSRMLS_CC);
			efree(msg);
			return 0;
		}

		DEBUG_C("%ld: call api '%s' at (%c)'%s' with '%d' parameters",
				request->id, request->method, (flags & YAR_PROTOCOL_PERSISTENT)? 'p' : 'r', entry->uri, 
			   	zend_hash_num_elements(Z_ARRVAL_P(request->parameters)));

		if (!transport->send(transport, request, &msg TSRMLS_CC)) {
			php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_TRANSPORT, msg TSRMLS_CC);
			transport->close(transport TSRMLS_CC);
			factory->destroy(transport TSRMLS_CC);
			efree(msg);
			return 0;
		}

		transport->calldata(transport, entry TSRMLS_CC);
		multi->add(multi, transport TSRMLS_CC);
		php_yar_request_destroy(request TSRMLS_CC);
	}

	if (!multi->exec(multi, php_yar_concurrent_client_callback TSRMLS_CC)) {
		multi->close(multi TSRMLS_CC);
		return 0;
	}

	multi->close(multi TSRMLS_CC);
	return 1;
} /* }}} */

/* {{{ proto Yar_Client::__construct($uri[, array $options = NULL]) */
PHP_METHOD(yar_client, __construct) {
	char *url;
	long len;
	zval *options = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a!", &url, &len, &options) == FAILURE) {
        return;
    }

    zend_update_property_stringl(yar_client_ce, getThis(), ZEND_STRL("_uri"), url, len TSRMLS_CC);

	if (strncasecmp(url, ZEND_STRL("http://")) == 0 || strncasecmp(url, ZEND_STRL("https://")) == 0) {
	} else if (strncasecmp(url, ZEND_STRL("tcp://")) == 0) {
		zend_update_property_long(yar_client_ce, getThis(), ZEND_STRL("_protocol"), YAR_CLIENT_PROTOCOL_TCP TSRMLS_CC);
	} else if (strncasecmp(url, ZEND_STRL("unix://")) == 0) {
		zend_update_property_long(yar_client_ce, getThis(), ZEND_STRL("_protocol"), YAR_CLIENT_PROTOCOL_UNIX TSRMLS_CC);
	} else {
		php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_PROTOCOL, "unsupported protocol address %s", url);
		return;
	}

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

	switch (Z_LVAL_P(protocol)) {
		case YAR_CLIENT_PROTOCOL_TCP:
		case YAR_CLIENT_PROTOCOL_UNIX:
		case YAR_CLIENT_PROTOCOL_HTTP:
			if ((ret = php_yar_client_handle(Z_LVAL_P(protocol), getThis(), method, mlen, params TSRMLS_CC))) {
				RETVAL_ZVAL(ret, 1, 1);
				return;
			}
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "unsupported protocol %ld", Z_LVAL_P(protocol));
			break;
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto Yar_Client::call($method, $parameters = NULL) */
PHP_METHOD(yar_client, call) {
	PHP_MN(yar_client___call)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ proto Yar_Client::getOpt(int $type) */
PHP_METHOD(yar_client, getOpt) {
	long type;
	zval *value;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz", &type, &value) == FAILURE) {
		return;
	} else {
		zval * options = zend_read_property(yar_client_ce, getThis(), ZEND_STRL("_options"), 0 TSRMLS_CC);
		if (!(value = php_yar_client_get_opt(options, type TSRMLS_CC))) {
			RETURN_FALSE;
		}

		RETURN_ZVAL(value, 1, 0);
	}
}
/* }}} */

/* {{{ proto Yar_Client::setOpt(int $type, mixed $value) */
PHP_METHOD(yar_client, setOpt) {
	long type;
	zval *value;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz", &type, &value) == FAILURE) {
		return;
	}

	if (!php_yar_client_set_opt(getThis(), type, value TSRMLS_CC)) {
		RETURN_FALSE;
	}

	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/* {{{ proto Yar_Concurrent_Client::call($uri, $method, $parameters = NULL, $callback = NULL, $error_callback = NULL, $options = array()) */
PHP_METHOD(yar_concurrent_client, call) {
	char *uri, *method, *name = NULL;
	long sequence, ulen = 0, mlen = 0;
	zval *callstack, *item, *status;
	zval *error_callback = NULL, *callback = NULL, *parameters = NULL, *options = NULL;
	yar_call_data_t *entry;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a!z!za",
				&uri, &ulen, &method, &mlen, &parameters, &callback, &error_callback, &options) == FAILURE) {
		return;
	}

	if (!ulen) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "first parameter is expected to be a valid rpc server uri");
		return;
	}

	if (strncasecmp(uri, ZEND_STRL("http://")) && strncasecmp(uri, ZEND_STRL("https://"))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "only http protocol is supported in concurrent client for now");
		return;
	}

	if (!mlen) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "second parameter is expected to be a valid rpc api name");
		return;
	}

    if (callback && !ZVAL_IS_NULL(callback) && !zend_is_callable(callback, 0, &name TSRMLS_CC)) {
        php_error_docref1(NULL TSRMLS_CC, name, E_ERROR, "fourth parameter is expected to be a valid callback");
        efree(name);
        RETURN_FALSE;
    }

	if (name) {
		efree(name);
		name = NULL;
	}

    if (error_callback && !ZVAL_IS_NULL(error_callback) && !zend_is_callable(error_callback, 0, &name TSRMLS_CC)) {
        php_error_docref1(NULL TSRMLS_CC, name, E_ERROR, "fifth parameter is expected to be a valid error callback");
        efree(name);
        RETURN_FALSE;
    }
	if (name) {
		efree(name);
	}

	status = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_start"), 0 TSRMLS_CC);
	if (Z_BVAL_P(status)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "concurrent client has already started");
		RETURN_FALSE;
	}


	entry = ecalloc(1, sizeof(yar_call_data_t));

	entry->uri = estrndup(uri, ulen);
	entry->ulen = ulen;
	entry->method = estrndup(method, mlen);
	entry->mlen = mlen;

	if (callback && !ZVAL_IS_NULL(callback)) {
		Z_ADDREF_P(callback);
		entry->callback = callback;
	}
	if (error_callback && !ZVAL_IS_NULL(error_callback)) {
		Z_ADDREF_P(error_callback);
		entry->ecallback = error_callback;
	}
	if (parameters && IS_ARRAY == Z_TYPE_P(parameters)) {
		Z_ADDREF_P(parameters);
		entry->parameters = parameters;
	}
	if (options && IS_ARRAY == Z_TYPE_P(options)) {
		Z_ADDREF_P(options);
		entry->options = options;
	}

	callstack = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callstack"), 0 TSRMLS_CC);
	if (ZVAL_IS_NULL(callstack)) {
		MAKE_STD_ZVAL(callstack);
		array_init(callstack);
		zend_update_static_property(yar_concurrent_client_ce, ZEND_STRL("_callstack"), callstack TSRMLS_CC);
		zval_ptr_dtor(&callstack);
	}

	MAKE_STD_ZVAL(item);

	ZEND_REGISTER_RESOURCE(item, entry, le_calldata);

	sequence = zend_hash_next_free_element(Z_ARRVAL_P(callstack));
	entry->sequence = sequence + 1;

	zend_hash_next_index_insert(Z_ARRVAL_P(callstack), &item, sizeof(zval *), NULL);

	RETURN_LONG(entry->sequence);
}
/* }}} */

/* {{{ proto Yar_Concurrent_Client::loop($callback = NULL, $error_callback) */
PHP_METHOD(yar_concurrent_client, loop) {
	char *name = NULL;
	zval *callstack;
	zval *callback = NULL, *error_callback = NULL;
	zval *status;
	uint ret = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zz", &callback, &error_callback) == FAILURE) {
		return;
	}

	status = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_start"), 0 TSRMLS_CC);
	if (Z_BVAL_P(status)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "concurrent client has already started");
		RETURN_FALSE;
	}

    if (callback && !ZVAL_IS_NULL(callback) && !zend_is_callable(callback, 0, &name TSRMLS_CC)) {
        php_error_docref1(NULL TSRMLS_CC, name, E_ERROR, "first argument is expected to be a valid callback");
        efree(name);
        RETURN_FALSE;
    }

	if (name) {
		efree(name);
		name = NULL;
	}

    if (error_callback && !ZVAL_IS_NULL(error_callback) && !zend_is_callable(error_callback, 0, &name TSRMLS_CC)) {
        php_error_docref1(NULL TSRMLS_CC, name, E_ERROR, "second argument is expected to be a valid error callback");
        efree(name);
        RETURN_FALSE;
    }
	if (name) {
		efree(name);
	}

	callstack = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callstack"), 0 TSRMLS_CC);
	if (ZVAL_IS_NULL(callstack) || zend_hash_num_elements(Z_ARRVAL_P(callstack)) == 0) {
		RETURN_TRUE;
	}

	if (callback && !ZVAL_IS_NULL(callback)) {
		zend_update_static_property(yar_concurrent_client_ce, ZEND_STRL("_callback"), callback TSRMLS_CC);
	}

	if (error_callback && !ZVAL_IS_NULL(error_callback)) {
		zend_update_static_property(yar_concurrent_client_ce, ZEND_STRL("_error_callback"), error_callback TSRMLS_CC);
	}

	ZVAL_BOOL(status, 1);
	ret = php_yar_concurrent_client_handle(callstack TSRMLS_CC);
	ZVAL_BOOL(status, 0);
	RETURN_BOOL(ret);
}
/* }}} */

/* {{{ yar_client_methods */
zend_function_entry yar_client_methods[] = {
	PHP_ME(yar_client, __construct, arginfo_client___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)
	PHP_ME(yar_client, call, arginfo_client___call, ZEND_ACC_PUBLIC)
	PHP_ME(yar_client, __call, arginfo_client___call, ZEND_ACC_PUBLIC)
	PHP_ME(yar_client, getOpt, arginfo_client_getopt, ZEND_ACC_PUBLIC)
	PHP_ME(yar_client, setOpt, arginfo_client_setopt, ZEND_ACC_PUBLIC)
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
	zend_declare_property_null(yar_client_ce, ZEND_STRL("_options"),  ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yar_client_ce, ZEND_STRL("_running"),  ZEND_ACC_PROTECTED TSRMLS_CC);

	INIT_CLASS_ENTRY(ce, "Yar_Concurrent_Client", yar_concurrent_client_methods);
    yar_concurrent_client_ce = zend_register_internal_class(&ce TSRMLS_CC);	
	zend_declare_property_null(yar_concurrent_client_ce, ZEND_STRL("_callstack"), ZEND_ACC_PROTECTED|ZEND_ACC_STATIC TSRMLS_CC);
	zend_declare_property_null(yar_concurrent_client_ce, ZEND_STRL("_callback"), ZEND_ACC_PROTECTED|ZEND_ACC_STATIC TSRMLS_CC);
	zend_declare_property_null(yar_concurrent_client_ce, ZEND_STRL("_error_callback"), ZEND_ACC_PROTECTED|ZEND_ACC_STATIC TSRMLS_CC);
	zend_declare_property_bool(yar_concurrent_client_ce, ZEND_STRL("_start"), 0, ZEND_ACC_PROTECTED|ZEND_ACC_STATIC TSRMLS_CC);

	REGISTER_LONG_CONSTANT("YAR_CLIENT_PROTOCOL_HTTP", YAR_CLIENT_PROTOCOL_HTTP, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("YAR_CLIENT_PROTOCOL_TCP", YAR_CLIENT_PROTOCOL_TCP, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("YAR_CLIENT_PROTOCOL_UNIX", YAR_CLIENT_PROTOCOL_UNIX, CONST_PERSISTENT | CONST_CS);

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
