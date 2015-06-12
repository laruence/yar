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

static void php_yar_client_trigger_error(int throw_exception, int code, const char *format, ...) /* {{{ */ {
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
		zend_throw_exception(ce, message, code);
	} else {
		php_error_docref(NULL, E_WARNING, "[%d] %s", code, message);
	}

	if (message) {
		efree(message);
	}
} /* }}} */

static void php_yar_client_handle_error(int throw_exception, yar_response_t *response) /* {{{ */ {
	if (response->status == YAR_ERR_EXCEPTION) {
		if (throw_exception) {
			zval ex, *property;
			object_init_ex(&ex, yar_server_exception_ce);

			if ((property = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("message"))) != NULL) {
				zend_update_property(yar_server_exception_ce, &ex, ZEND_STRL("message"), property);
			}

			if ((property = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("code"))) != NULL) {
				zend_update_property(yar_server_exception_ce, &ex, ZEND_STRL("code"), property);
			}

			if ((property = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("file"))) != NULL) {
				zend_update_property(yar_server_exception_ce, &ex, ZEND_STRL("file"), property);
			}

			if ((property = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("line"))) != NULL) {
				zend_update_property(yar_server_exception_ce, &ex, ZEND_STRL("line"), property);
			}

			if ((property = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("_type"))) != NULL) {
				zend_update_property(yar_server_exception_ce, &ex, ZEND_STRL("_type"), property);
			}

			zend_throw_exception_object(&ex);
		} else {
			zval *msg, *code;
			if ((msg = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("message"))) != NULL
					&& (code = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("code"))) != NULL) {
				convert_to_string_ex(msg);
				convert_to_long_ex(code);
				php_yar_client_trigger_error(0, Z_LVAL_P(code), "server threw an exception with message `%s`", Z_STRVAL_P(msg));
			}
		}
	} else {
		php_yar_client_trigger_error(throw_exception, response->status, "%s", Z_STRVAL(response->err));
	}
}
/* }}} */

static zval * php_yar_client_get_opt(zval *options, long type) /* {{{ */ {
	zval *value;
	if (IS_ARRAY != Z_TYPE_P(options)) {
		return NULL;
	}

	if ((value = zend_hash_index_find(Z_ARRVAL_P(options), type)) != NULL) {
		return value;
	}

	return NULL;
} /* }}} */

static int php_yar_client_set_opt(zval *client, long type, zval *value) /* {{{ */ {
	zend_bool verified = 0;
	zval rv;

	switch (type) {
		case YAR_OPT_PACKAGER:
		{
			 verified = 1;
             if (IS_STRING != Z_TYPE_P(value)) {
				 php_error_docref(NULL, E_WARNING, "expects a string packager name");
				 return 0;
			 }
		}
		case YAR_OPT_PERSISTENT:
		{
			if (!verified) {
				verified = 1;
				if (IS_LONG != Z_TYPE_P(value) && IS_TRUE != Z_TYPE_P(value) && IS_FALSE != Z_TYPE_P(value)) {
					php_error_docref(NULL, E_WARNING, "expects a boolean persistent flag");
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
					php_error_docref(NULL, E_WARNING, "expects a long integer timeout value");
					return 0;
				}
			}

			options = zend_read_property(yar_client_ce, client, ZEND_STRL("_options"), 0, &rv);
			if (IS_ARRAY != Z_TYPE_P(options)) {
			    zval tmp_options;
				array_init(&tmp_options);
				zend_update_property(yar_client_ce, client, ZEND_STRL("_options"), &tmp_options);
				zval_ptr_dtor(&tmp_options);
			}

			Z_TRY_ADDREF_P(value);
			zend_hash_index_update(Z_ARRVAL_P(options), type, value);
			break;
		}
		default:
			return 0;
	}

	return 1;
} /* }}} */

static int php_yar_client_handle(int protocol, zval *client, zend_string *method, zval *params, zval *retval) /* {{{ */ {
	char *msg;
	zval *uri, *options;
	zval rv;
	yar_transport_t *factory;
	yar_transport_interface_t *transport;
	yar_request_t *request;
	yar_response_t *response;
	int flags = 0;

	uri = zend_read_property(yar_client_ce, client, ZEND_STRL("_uri"), 0, &rv);

	if (protocol == YAR_CLIENT_PROTOCOL_HTTP) {
		factory = php_yar_transport_get(ZEND_STRL("curl"));
	} else if (protocol == YAR_CLIENT_PROTOCOL_TCP || protocol == YAR_CLIENT_PROTOCOL_UNIX) {
		factory = php_yar_transport_get(ZEND_STRL("sock"));
	} else {
		return 0;
	}

	transport = factory->init();

	options = zend_read_property(yar_client_ce, client, ZEND_STRL("_options"), 1, &rv);

	if (IS_ARRAY != Z_TYPE_P(options)) {
		options = NULL;
	}

	if (!(request = php_yar_request_instance(method, params, options))) {
		transport->close(transport);
		factory->destroy(transport);
		return 0;
	}

	if (YAR_G(allow_persistent)) {
		if (options) {
			zval *flag = php_yar_client_get_opt(options, YAR_OPT_PERSISTENT);
			if (flag && (Z_TYPE_P(flag) == IS_TRUE || (Z_TYPE_P(flag) == IS_LONG && Z_LVAL_P(flag)))) {
				flags |= YAR_PROTOCOL_PERSISTENT;
			}
		}
	}

	if (!transport->open(transport, Z_STR_P(uri), flags, &msg)) {
		php_yar_client_trigger_error(1, YAR_ERR_TRANSPORT, msg);
		php_yar_request_destroy(request);
		efree(msg);
		return 0;
	}

	DEBUG_C("%ld: call api '%s' at (%c)'%s' with '%d' parameters",
			request->id, request->method->val, (flags & YAR_PROTOCOL_PERSISTENT)? 'p' : 'r', Z_STRVAL_P(uri), 
			zend_hash_num_elements(Z_ARRVAL(request->parameters)));

	if (!transport->send(transport, request, &msg)) {
		php_yar_client_trigger_error(1, YAR_ERR_TRANSPORT, msg);
		php_yar_request_destroy(request);
		efree(msg);
		return 0;
	}

	response = transport->exec(transport, request);

	if (response->status != YAR_ERR_OKEY) {
		php_yar_client_handle_error(1, response);
		php_yar_request_destroy(request);
		php_yar_response_destroy(response);
		transport->close(transport);
		factory->destroy(transport);
		return 0;
	} else {
		if (response->out && response->out->len) {
			PHPWRITE(response->out->val, response->out->len);
		}
		ZVAL_COPY(retval, &response->retval);
		php_yar_request_destroy(request);
		php_yar_response_destroy(response);
		transport->close(transport);
		factory->destroy(transport);
		return 1;
	}
} /* }}} */

int php_yar_concurrent_client_callback(yar_call_data_t *calldata, int status, yar_response_t *response) /* {{{ */ {
	zval code, retval, retval_ptr;
	zval callinfo, *callback, *func_params;
	zend_bool bailout = 0;
	uint params_count, i;

	if (calldata) {
		/* data callback */
		if (status == YAR_ERR_OKEY) {
			if (!Z_ISUNDEF(calldata->callback)) {
				callback = &calldata->callback;
			} else {
				callback = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callback"), 0);
			}
			params_count = 2;
		} else {
			if (!Z_ISUNDEF(calldata->ecallback)) {
				callback = &calldata->ecallback;
			} else {
				callback = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_error_callback"), 0);
			}
			params_count = 3;
		}

		if (Z_ISNULL_P(callback)) {
			if (status != YAR_ERR_OKEY) {
				if (!Z_ISUNDEF(response->err)) {
					php_yar_client_handle_error(0, response);
				} else {
					php_error_docref(NULL, E_WARNING, "[%d]:unknown Error", status);
				}
			} else if (!Z_ISUNDEF(response->retval)) {
				zend_print_zval(&response->retval, 1);
			}
			return 1;
		}

		if (status == YAR_ERR_OKEY) {
			if (Z_ISUNDEF(response->retval)) {
				php_yar_client_trigger_error(0, YAR_ERR_REQUEST, "%s", "server responsed empty response");
				return 1;
			}
			ZVAL_COPY(&retval, &response->retval);
		} else {
			ZVAL_LONG(&code, status);
			ZVAL_COPY(&retval, &response->err);
		}

		array_init(&callinfo);

		add_assoc_long_ex(&callinfo, "sequence", sizeof("sequence") - 1, calldata->sequence);
		add_assoc_str_ex(&callinfo, "uri", sizeof("uri") - 1, zend_string_copy(calldata->uri));
		add_assoc_str_ex(&callinfo, "method", sizeof("method") - 1, zend_string_copy(calldata->method));
	} else {
		callback = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callback"), 0);
		if (Z_ISNULL_P(callback)) {
			return 1;
		}
		params_count = 2;
	}

	if (calldata && (status != YAR_ERR_OKEY)) {
		func_params = safe_emalloc(sizeof(zval), 3, 0);
	    ZVAL_COPY_VALUE(&func_params[0], &code);
	    ZVAL_COPY_VALUE(&func_params[1], &retval);
	    ZVAL_COPY_VALUE(&func_params[2], &callinfo);
	} else if (calldata) {
		func_params = safe_emalloc(sizeof(zval), 2, 0);
	    ZVAL_COPY_VALUE(&func_params[0], &retval);
	    ZVAL_COPY_VALUE(&func_params[1], &callinfo);
	} else {
		func_params = safe_emalloc(sizeof(zval), 2, 0);
	    ZVAL_NULL(&func_params[0]);
	    ZVAL_NULL(&func_params[1]);
	}

	zend_try {
		if (call_user_function_ex(EG(function_table), NULL, callback,
					&retval_ptr, params_count, func_params, 0, NULL) != SUCCESS) {
			for (i = 0; i < params_count; i++) {
				zval_ptr_dtor(&func_params[i]);	
			}
			efree(func_params);
			if (calldata) {
				php_error_docref(NULL, E_WARNING, "call to callback failed for request: '%s'", calldata->method);
			} else {
				php_error_docref(NULL, E_WARNING, "call to initial callback failed");
			}
			return 1;
		}
	} zend_catch {
		bailout = 1;
	} zend_end_try();

	if (!Z_ISUNDEF(retval_ptr)) {
		zval_ptr_dtor(&retval_ptr);
	}

	for (i = 0; i < params_count; i++) {
		zval_ptr_dtor(&func_params[i]);	
	}
	efree(func_params);
    return bailout? 0 : 1;
} /* }}} */

int php_yar_concurrent_client_handle(zval *callstack) /* {{{ */ {
	char *msg;
	zend_string *dummy;
	zend_ulong sequence;
	zval *calldata;
	yar_request_t *request;
	yar_transport_t *factory;
	yar_transport_interface_t *transport;
	yar_transport_multi_interface_t *multi;

	factory = php_yar_transport_get(ZEND_STRL("curl"));
	multi = factory->multi->init();

    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(callstack), sequence, dummy, calldata) {
		yar_call_data_t *entry;
		long flags = 0;

		entry = (yar_call_data_t *)zend_fetch_resource(Z_RES_P(calldata), "Yar Call Data", le_calldata);

		if (!entry) {
			continue;
		}

		if (Z_ISUNDEF(entry->parameters)) {
			array_init(&entry->parameters);
		} 

		transport = factory->init();

		if (YAR_G(allow_persistent)) {
			if (!Z_ISUNDEF(entry->options)) {
				zval *flag = php_yar_client_get_opt(&entry->options, YAR_OPT_PERSISTENT);
				if (flag && (Z_TYPE_P(flag) == IS_TRUE || (Z_TYPE_P(flag) == IS_LONG && Z_LVAL_P(flag)))) {
					flags |= YAR_PROTOCOL_PERSISTENT;
				}
			}
		}

		if (!(request = php_yar_request_instance(entry->method,
						&entry->parameters, Z_ISUNDEF(entry->options)? NULL: & entry->options))) {
			transport->close(transport);
			factory->destroy(transport);
			return 0;
		}

		if (!transport->open(transport, entry->uri, flags, &msg)) {
			php_yar_client_trigger_error(1, YAR_ERR_TRANSPORT, msg);
			transport->close(transport);
			factory->destroy(transport);
			efree(msg);
			return 0;
		}

		DEBUG_C("%ld: call api '%s' at (%c)'%s' with '%d' parameters",
				request->id, request->method->val, (flags & YAR_PROTOCOL_PERSISTENT)? 'p' : 'r', entry->uri, 
			   	zend_hash_num_elements(Z_ARRVAL(request->parameters)));

		if (!transport->send(transport, request, &msg)) {
			php_yar_client_trigger_error(1, YAR_ERR_TRANSPORT, msg);
			transport->close(transport);
			factory->destroy(transport);
			efree(msg);
			return 0;
		}

		transport->calldata(transport, entry);
		multi->add(multi, transport);
		php_yar_request_destroy(request);
	} ZEND_HASH_FOREACH_END();

	if (!multi->exec(multi, php_yar_concurrent_client_callback)) {
		multi->close(multi);
		return 0;
	}

	multi->close(multi);
	return 1;
} /* }}} */

/* {{{ proto Yar_Client::__construct($uri[, array $options = NULL]) */
PHP_METHOD(yar_client, __construct) {
	zend_string *url;
	zval *options = NULL;

    if (zend_parse_parameters_throw(ZEND_NUM_ARGS(), "S|a!", &url, &options) == FAILURE) {
        return;
    }

    zend_update_property_str(yar_client_ce, getThis(), ZEND_STRL("_uri"), url);

	if (strncasecmp(url->val, "http://", sizeof("http://") - 1) == 0
			|| strncasecmp(url->val, "https://", sizeof("https://") - 1) == 0) {
	} else if (strncasecmp(url->val, "tcp://", sizeof("tcp://") - 1) == 0) {
		zend_update_property_long(yar_client_ce, getThis(), ZEND_STRL("_protocol"), YAR_CLIENT_PROTOCOL_TCP);
	} else if (strncasecmp(url->val, "unix://", sizeof("unix://") - 1) == 0) {
		zend_update_property_long(yar_client_ce, getThis(), ZEND_STRL("_protocol"), YAR_CLIENT_PROTOCOL_UNIX);
	} else {
		php_yar_client_trigger_error(1, YAR_ERR_PROTOCOL, "unsupported protocol address %s", url->val);
		return;
	}

	if (options) {
    	zend_update_property(yar_client_ce, getThis(), ZEND_STRL("_options"), options);
	}
}
/* }}} */

/* {{{ proto Yar_Client::__call($method, $parameters = NULL) */
PHP_METHOD(yar_client, __call) {
	zval *params, *protocol, rv;
	zend_string *method;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sa", &method, &params) == FAILURE) {
		return;
	}

	protocol = zend_read_property(yar_client_ce, getThis(), ZEND_STRL("_protocol"), 0, &rv);

	switch (Z_LVAL_P(protocol)) {
		case YAR_CLIENT_PROTOCOL_TCP:
		case YAR_CLIENT_PROTOCOL_UNIX:
		case YAR_CLIENT_PROTOCOL_HTTP:
			if ((php_yar_client_handle(Z_LVAL_P(protocol), getThis(), method, params, return_value))) {
				return;
			}
			break;
		default:
			php_error_docref(NULL, E_WARNING, "unsupported protocol %ld", Z_LVAL_P(protocol));
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
	zval *value, rv;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "lz", &type, &value) == FAILURE) {
		return;
	} else {
		zval * options = zend_read_property(yar_client_ce, getThis(), ZEND_STRL("_options"), 0, &rv);
		if (!(value = php_yar_client_get_opt(options, type))) {
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

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "lz", &type, &value) == FAILURE) {
		return;
	}

	if (!php_yar_client_set_opt(getThis(), type, value)) {
		RETURN_FALSE;
	}

	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/* {{{ proto Yar_Concurrent_Client::call($uri, $method, $parameters = NULL, $callback = NULL, $error_callback = NULL, $options = array()) */
PHP_METHOD(yar_concurrent_client, call) {
	zend_string *uri, *method;
    zend_string *name = NULL;
	long sequence;
	zval *callstack, item, *status;
	zval *error_callback = NULL, *callback = NULL, *parameters = NULL, *options = NULL;
	yar_call_data_t *entry;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|a!z!za",
				&uri, &method, &parameters, &callback, &error_callback, &options) == FAILURE) {
		return;
	}

	if (!uri->len) {
		php_error_docref(NULL, E_WARNING, "first parameter is expected to be a valid rpc server uri");
		return;
	}

	if (strncasecmp(uri->val, "http://", sizeof("http://") - 1) 
			&& strncasecmp(uri->val, "https://", sizeof("https://") - 1)) {
		php_error_docref(NULL, E_WARNING, "only http protocol is supported in concurrent client for now");
		return;
	}

	if (!method->len) {
		php_error_docref(NULL, E_WARNING, "second parameter is expected to be a valid rpc api name");
		return;
	}

    if (callback && !Z_ISNULL_P(callback) &&
			!zend_is_callable(callback, 0, &name)) {
        php_error_docref1(NULL, name->val, E_ERROR, "fourth parameter is expected to be a valid callback");
        zend_string_release(name);
        RETURN_FALSE;
    }

	if (name) {
		zend_string_release(name);
		name = NULL;
	}

    if (error_callback && !Z_ISNULL_P(error_callback) &&
			!zend_is_callable(error_callback, 0, &name)) {
        php_error_docref1(NULL, name->val, E_ERROR, "fifth parameter is expected to be a valid error callback");
        zend_string_release(name);
        RETURN_FALSE;
    }

	if (name) {
        zend_string_release(name);
	}

	status = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_start"), 0);
	if (Z_TYPE_P(status) == IS_TRUE) {
        php_error_docref(NULL, E_WARNING, "concurrent client has already started");
		RETURN_FALSE;
	}

	entry = ecalloc(1, sizeof(yar_call_data_t));

	entry->uri = zend_string_copy(uri);
	entry->method = zend_string_copy(method);

	if (callback && !Z_ISNULL_P(callback)) {
		ZVAL_COPY(&entry->callback, callback);
	}
	if (error_callback && !Z_ISNULL_P(error_callback)) {
		ZVAL_COPY(&entry->ecallback, error_callback);
	}
	if (parameters && IS_ARRAY == Z_TYPE_P(parameters)) {
		ZVAL_COPY(&entry->parameters, parameters);
	}
	if (options && IS_ARRAY == Z_TYPE_P(options)) {
		ZVAL_COPY(&entry->options, options);
	}

	callstack = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callstack"), 0);
	if (Z_ISNULL_P(callstack)) {
	    zval tmp;
		array_init(&tmp);
		zend_update_static_property(yar_concurrent_client_ce, ZEND_STRL("_callstack"), &tmp);
		zval_ptr_dtor(&tmp);
	}

	ZVAL_RES(&item, zend_register_resource(entry, le_calldata));

	sequence = zend_hash_next_free_element(Z_ARRVAL_P(callstack));
	entry->sequence = sequence + 1;

	zend_hash_next_index_insert(Z_ARRVAL_P(callstack), &item);

	RETURN_LONG(entry->sequence);
}
/* }}} */

/* {{{ proto Yar_Concurrent_Client::reset(void) */
PHP_METHOD(yar_concurrent_client, reset) {
	zval *callstack;


	callstack = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callstack"), 0);
	if (Z_ISNULL_P(callstack) || zend_hash_num_elements(Z_ARRVAL_P(callstack)) == 0) {
		RETURN_TRUE;
	}
	zend_hash_clean(Z_ARRVAL_P(callstack));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto Yar_Concurrent_Client::loop($callback = NULL, $error_callback) */
PHP_METHOD(yar_concurrent_client, loop) {
	zend_string *name = NULL;
	zval *callstack;
	zval *callback = NULL, *error_callback = NULL;
	zval *status;
	uint ret = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|zz", &callback, &error_callback) == FAILURE) {
		return;
	}

	status = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_start"), 0);
	if (Z_TYPE_P(status) == IS_TRUE) {
        php_error_docref(NULL, E_WARNING, "concurrent client has already started");
		RETURN_FALSE;
	}

    if (callback && !Z_ISNULL_P(callback) &&
			!zend_is_callable(callback, 0, &name)) {
        php_error_docref1(NULL, name->val, E_ERROR, "first argument is expected to be a valid callback");
        zend_string_release(name);
        RETURN_FALSE;
    }

	if (name) {
		zend_string_release(name);
		name = NULL;
	}

    if (error_callback && !Z_ISNULL_P(error_callback) &&
			!zend_is_callable(error_callback, 0, &name)) {
        php_error_docref1(NULL, name->val, E_ERROR, "second argument is expected to be a valid error callback");
        zend_string_release(name);
        RETURN_FALSE;
    }
	if (name) {
		zend_string_release(name);
	}

	callstack = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callstack"), 0);
	if (Z_ISNULL_P(callstack) || zend_hash_num_elements(Z_ARRVAL_P(callstack)) == 0) {
		RETURN_TRUE;
	}

	if (callback && !Z_ISNULL_P(callback)) {
		zend_update_static_property(yar_concurrent_client_ce, ZEND_STRL("_callback"), callback);
	}

	if (error_callback && !Z_ISNULL_P(error_callback)) {
		zend_update_static_property(yar_concurrent_client_ce, ZEND_STRL("_error_callback"), error_callback);
	}

	ZVAL_BOOL(status, 1);
	ret = php_yar_concurrent_client_handle(callstack);
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
	PHP_ME(yar_concurrent_client, reset, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_FE_END
};
/* }}} */

YAR_STARTUP_FUNCTION(client) /* {{{ */ {
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "Yar_Client", yar_client_methods);
    yar_client_ce = zend_register_internal_class(&ce);

	zend_declare_property_long(yar_client_ce, ZEND_STRL("_protocol"), YAR_CLIENT_PROTOCOL_HTTP, ZEND_ACC_PROTECTED);
	zend_declare_property_null(yar_client_ce, ZEND_STRL("_uri"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(yar_client_ce, ZEND_STRL("_options"),  ZEND_ACC_PROTECTED);
	zend_declare_property_null(yar_client_ce, ZEND_STRL("_running"),  ZEND_ACC_PROTECTED);

	INIT_CLASS_ENTRY(ce, "Yar_Concurrent_Client", yar_concurrent_client_methods);
    yar_concurrent_client_ce = zend_register_internal_class(&ce);	
	zend_declare_property_null(yar_concurrent_client_ce, ZEND_STRL("_callstack"), ZEND_ACC_PROTECTED|ZEND_ACC_STATIC);
	zend_declare_property_null(yar_concurrent_client_ce, ZEND_STRL("_callback"), ZEND_ACC_PROTECTED|ZEND_ACC_STATIC);
	zend_declare_property_null(yar_concurrent_client_ce, ZEND_STRL("_error_callback"), ZEND_ACC_PROTECTED|ZEND_ACC_STATIC);
	zend_declare_property_bool(yar_concurrent_client_ce, ZEND_STRL("_start"), 0, ZEND_ACC_PROTECTED|ZEND_ACC_STATIC);

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
