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

#if PHP_MAJOR_VERSION > 7
#include "yar_arginfo.h"
#else
#include "yar_legacy_arginfo.h"
#endif


#include <curl/curl.h> /* For checking CUROPT_RESOLVE */

zend_class_entry *yar_client_ce;
zend_class_entry *yar_concurrent_client_ce;

static zend_object_handlers yar_client_obj_handlers;

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
			case YAR_ERR_EXCEPTION:
				ce = yar_server_exception_ce;
				break;
			case YAR_ERR_REQUEST:
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
	if (response->status == YAR_ERR_EXCEPTION && Z_TYPE(response->err) == IS_ARRAY) {
		if (throw_exception) {
			zval ex, *property;
			object_init_ex(&ex, yar_server_exception_ce);
#if PHP_VERSION_ID < 80000
			zval *obj = &ex;
#else
			zend_object *obj = Z_OBJ(ex);
#endif

			if ((property = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("message"))) != NULL) {
				zend_update_property(yar_server_exception_ce, obj, ZEND_STRL("message"), property);
			}

			if ((property = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("code"))) != NULL) {
				zend_update_property(yar_server_exception_ce, obj, ZEND_STRL("code"), property);
			}

			if ((property = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("file"))) != NULL) {
				zend_update_property(yar_server_exception_ce, obj, ZEND_STRL("file"), property);
			}

			if ((property = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("line"))) != NULL) {
				zend_update_property(yar_server_exception_ce, obj, ZEND_STRL("line"), property);
			}

			if ((property = zend_hash_str_find(Z_ARRVAL(response->err), ZEND_STRL("_type"))) != NULL) {
				zend_update_property(yar_server_exception_ce, obj, ZEND_STRL("_type"), property);
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

static inline int php_yar_check_array_values(zend_array *arr) /* {{{ */ {
	zval *rv;
	ZEND_HASH_FOREACH_VAL(arr, rv) {
		ZVAL_DEREF(rv);
		if (Z_TYPE_P(rv) != IS_STRING) {
			return 0;
		}
	} ZEND_HASH_FOREACH_END();
	return 1;
}
/* }}} */

static void php_yar_options_dtor(void **options) /* {{{ */ {
	unsigned int i;
	for (i = 0; i < YAR_OPT_MAX; i++) {
		if (options[i]) {
			switch (i) {
				case YAR_OPT_PACKAGER:
				case YAR_OPT_PROXY:
				case YAR_OPT_PROVIDER:
				case YAR_OPT_TOKEN: {
					zend_string *val = (zend_string*)options[i];
					zend_string_release(val);
				}
				break;
				case YAR_OPT_HEADER:
				case YAR_OPT_RESOLVE: {
					zend_array *val = (zend_array*)options[i];
					zend_array_destroy(val);
				}
				break;
				default:
				break;
			}
		}
	}
	efree(options);
}
/* }}} */

static void php_yar_calldata_dtor(yar_call_data_t *entry) /* {{{ */ {
	if (entry->uri) {
		zend_string_release(entry->uri);
	}

	if (entry->method) {
		zend_string_release(entry->method);
	}

	if (entry->parameters) {
		zend_array_destroy(entry->parameters);
	}

	if (entry->options) {
		php_yar_options_dtor(entry->options);
	}

	if (entry->callback.fci.size) {
		zval_ptr_dtor(&entry->callback.fci.function_name);
	}

	if (entry->ecallback.fci.size) {
		zval_ptr_dtor(&entry->ecallback.fci.function_name);
	}

	efree(entry);
}
/* }}} */

static void php_yar_calllist_dtor() /* {{{ */ {
	yar_call_data_t *entry = (yar_call_data_t*)YAR_G(cctx).clist;
	while (entry) {
		yar_call_data_t *next = entry->next;
		php_yar_calldata_dtor(entry);
		entry = next;
	}
	YAR_G(cctx).clist = NULL;
}
/* }}} */

static int php_yar_client_get_opt(void **options, yar_opt type, zval *rv) /* {{{ */ {
	if (options == NULL || options[type] == NULL) {
		return 0;
	}

	switch (type) {
		case YAR_OPT_PACKAGER:
		case YAR_OPT_PROXY:
		case YAR_OPT_TOKEN:
		case YAR_OPT_PROVIDER: {
			ZVAL_STR_COPY(rv, (zend_string*)(options[type]));
			return 1;
		}
		case YAR_OPT_PERSISTENT:
		case YAR_OPT_TIMEOUT:
		case YAR_OPT_CONNECT_TIMEOUT: {
			ZVAL_LONG(rv, (zend_long)(options[type]));
			return 1;
		}
		case YAR_OPT_RESOLVE:
#if LIBCURL_VERSION_NUM < 0x071503
			return 0;
#endif
		case YAR_OPT_HEADER: {
			ZVAL_ARR(rv, zend_array_dup((zend_array*)(options[type])));
			return 1;
		}
		default:
		break;
	}
	return 0;
} /* }}} */

static int php_yar_client_validate_option(int protocol, yar_opt type) /* {{{ */ {
	switch (type) {
		case YAR_OPT_HEADER:
			if (protocol != YAR_CLIENT_PROTOCOL_HTTP) {
				php_error_docref(NULL, E_WARNING, "header only works with HTTP protocol");
				return 0;
			}
			break;
		case YAR_OPT_RESOLVE:
			if (protocol != YAR_CLIENT_PROTOCOL_HTTP) {
				php_error_docref(NULL, E_WARNING, "resolve only works with HTTP protocol");
				return 0;
			}
			break;
		case YAR_OPT_PROXY:
			if (protocol != YAR_CLIENT_PROTOCOL_HTTP) {
				php_error_docref(NULL, E_WARNING, "proxy only works with HTTP protocol");
				return 0;
			}
			break;
		default:
			break;
	}
	return 1;
}
/* }}} */

static int php_yar_client_set_opt(void **options, yar_opt type, zval *value) /* {{{ */ {
	switch (type) {
		case YAR_OPT_PACKAGER: {
		    if (IS_STRING != Z_TYPE_P(value)) {
			    php_error_docref(NULL, E_WARNING, "expects a string packager name");
			    return 0;
		    }
		    options[YAR_OPT_PACKAGER] = (void*)zend_string_copy(Z_STR_P(value));
		    return 1;
		}
		break;
		case YAR_OPT_PERSISTENT: {
			if (IS_LONG != Z_TYPE_P(value) && IS_TRUE != Z_TYPE_P(value) && IS_FALSE != Z_TYPE_P(value)) {
				php_error_docref(NULL, E_WARNING, "expects a boolean persistent flag");
				return 0;
			}
			options[YAR_OPT_PERSISTENT] = (void*)(zend_uintptr_t)(zval_get_long(value));
		}
		break;
		case YAR_OPT_HEADER: {
			if (IS_ARRAY != Z_TYPE_P(value)) {
				php_error_docref(NULL, E_WARNING, "expects an array as header value");
				return 0;
			}
			if (!php_yar_check_array_values(Z_ARRVAL_P(value))) {
				php_error_docref(NULL, E_WARNING, "expects an array which should only contains string value");
				return 0;
			}
			options[YAR_OPT_HEADER] = zend_array_dup(Z_ARRVAL_P(value));
			return 1;
		}
		break;
		case YAR_OPT_RESOLVE: {
			if (IS_ARRAY != Z_TYPE_P(value)) {
				php_error_docref(NULL, E_WARNING, "expects an array as resolve value");
				return 0;
			}
			if (!php_yar_check_array_values(Z_ARRVAL_P(value))) {
				php_error_docref(NULL, E_WARNING, "expects an array which should only contains string value");
				return 0;
			}
#if LIBCURL_VERSION_NUM < 0x071503
			/* Available since 7.21.3 */
			php_error_docref(NULL, E_WARNING, "YAR_OPT_RESOLVE require libcurl 7.21.3 and above");
			return 0;
#endif
			options[YAR_OPT_RESOLVE] = zend_array_dup(Z_ARRVAL_P(value));
			return 1;
		}
		break;
		case YAR_OPT_PROXY: {
			if (IS_STRING != Z_TYPE_P(value)){
				php_error_docref(NULL, E_WARNING, "expects a string as proxy value");
				return 0;
			}
			options[YAR_OPT_PROXY] = zend_string_copy(Z_STR_P(value));
		}
		break;
		case YAR_OPT_TIMEOUT:
		case YAR_OPT_CONNECT_TIMEOUT: {
			if (IS_LONG != Z_TYPE_P(value)) {
				php_error_docref(NULL, E_WARNING, "expects a long integer timeout value");
				return 0;
			}
			options[type] = (void*)(zend_uintptr_t)(zval_get_long(value));
		}
		break;
		case YAR_OPT_TOKEN:
		case YAR_OPT_PROVIDER: {
		    if (IS_STRING != Z_TYPE_P(value) || Z_STRLEN_P(value) > 32) {
				php_error_docref(NULL, E_WARNING, "expects a maximum 32 bytes string value");
				return 0;
			}
			options[type] = zend_string_copy(Z_STR_P(value));
		}
		break;
		default:
			return 0;
	}

	return 1;
} /* }}} */

static int php_yar_client_handle(yar_client_object *client, zend_string *method, zend_array *params, zval *retval) /* {{{ */ {
	char *msg;
	zend_string *uri;
	const yar_transport_t *factory;
	yar_transport_interface_t *transport;
	yar_request_t *request;
	yar_response_t *response;
	int flags = 0;

	if (client->protocol == YAR_CLIENT_PROTOCOL_HTTP) {
		factory = php_yar_transport_get(ZEND_STRL("curl"));
	} else if (client->protocol == YAR_CLIENT_PROTOCOL_TCP || client->protocol == YAR_CLIENT_PROTOCOL_UNIX) {
		factory = php_yar_transport_get(ZEND_STRL("sock"));
	} else {
		return 0;
	}

	uri = client->uri;
	transport = factory->init();

	if (UNEXPECTED(!(request = php_yar_request_instance(method, params, client->options)))) {
		transport->close(transport);
		factory->destroy(transport);
		return 0;
	}

	if (client->options && client->options[YAR_OPT_PERSISTENT]) {
		flags |= YAR_PROTOCOL_PERSISTENT;
	}

	/* This is tricky to pass options in, for custom headers */
	msg = (char*)client->options;
	if (UNEXPECTED(!transport->open(transport, uri, flags, &msg))) {
		php_yar_client_trigger_error(1, YAR_ERR_TRANSPORT, msg);
		php_yar_request_destroy(request);
		ZEND_ASSERT(msg != (char*)client->options);
		efree(msg);
		transport->close(transport);
		factory->destroy(transport);
		return 0;
	}

	DEBUG_C(ZEND_ULONG_FMT": call api '%s' at (%c)'%s' with '%d' parameters",
			request->id, ZSTR_VAL(request->method), (flags & YAR_PROTOCOL_PERSISTENT)? 'p' : 'r', ZSTR_VAL(uri),
			zend_hash_num_elements(request->parameters));

	if (UNEXPECTED(!transport->send(transport, request, &msg))) {
		php_yar_client_trigger_error(1, YAR_ERR_TRANSPORT, msg);
		php_yar_request_destroy(request);
		efree(msg);
		transport->close(transport);
		factory->destroy(transport);
		return 0;
	}

	response = transport->exec(transport, request);

	if (UNEXPECTED(response->status != YAR_ERR_OKEY)) {
		php_yar_client_handle_error(1, response);
		php_yar_request_destroy(request);
		php_yar_response_destroy(response);
		transport->close(transport);
		factory->destroy(transport);
		return 0;
	} else {
		if (response->out && ZSTR_LEN(response->out)) {
			PHPWRITE(ZSTR_VAL(response->out), ZSTR_LEN(response->out));
		}
		if (!Z_ISUNDEF(response->retval)) {
			ZVAL_COPY(retval, &response->retval);
		} else {
			/* server return nothing, probably called exit */
			ZVAL_NULL(retval);
		}
		php_yar_request_destroy(request);
		php_yar_response_destroy(response);
		transport->close(transport);
		factory->destroy(transport);
	}
	return 1;
} /* }}} */

int php_yar_concurrent_client_callback(yar_call_data_t *calldata, int status, yar_response_t *response) /* {{{ */ {
	zend_fcall_info *fci;
	zend_fcall_info_cache *fcc;
	zval retval_ptr;
	zval func_params[3];
	zend_bool bailout = 0;
	unsigned params_count, i;

	if (calldata) {
		zval callinfo;

		/* data callback */
		if (status == YAR_ERR_OKEY) {
			if (calldata->callback.fci.size) {
				fci = &calldata->callback.fci;
				fcc = &calldata->callback.fcc;
			} else {
				fci = &YAR_G(cctx).callback.fci;
				fcc = &YAR_G(cctx).callback.fcc;
			}
		} else {
			if (calldata->ecallback.fci.size) {
				fci = &calldata->ecallback.fci;
				fcc = &calldata->ecallback.fcc;
			} else {
				fci = &YAR_G(cctx).ecallback.fci;
				fcc = &YAR_G(cctx).ecallback.fcc;
			}
		}

		if (fci->size == 0) {
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

		if (UNEXPECTED(status == YAR_ERR_OKEY && Z_ISUNDEF(response->retval))) {
			php_yar_client_trigger_error(0, YAR_ERR_REQUEST, "%s", "server responsed empty response");
			return 1;
		}

		array_init(&callinfo);
		add_assoc_long_ex(&callinfo, "sequence", sizeof("sequence") - 1, calldata->sequence);
		add_assoc_str_ex(&callinfo, "uri", sizeof("uri") - 1, zend_string_copy(calldata->uri));
		add_assoc_str_ex(&callinfo, "method", sizeof("method") - 1, zend_string_copy(calldata->method));

		if (status == YAR_ERR_OKEY) {
			params_count = 2;
			ZVAL_COPY(&func_params[0], &response->retval);
			ZVAL_COPY_VALUE(&func_params[1], &callinfo);
		} else {
			params_count = 3;
			ZVAL_LONG(&func_params[0], status);
			ZVAL_COPY(&func_params[1], &response->err);
			ZVAL_COPY_VALUE(&func_params[2], &callinfo);
		}
	} else {
		/* first callback, triggered right after all request are sent */
		if (YAR_G(cctx).callback.fci.size == 0) {
			return 1;
		}
		fci = &YAR_G(cctx).callback.fci;
		fcc = &YAR_G(cctx).callback.fcc;

		params_count = 2;
	    ZVAL_NULL(&func_params[0]);
	    ZVAL_NULL(&func_params[1]);
	}

	zend_try {
		fci->param_count = params_count;
		fci->params = func_params;
		fci->retval = &retval_ptr;
#if PHP_VERSION_ID < 80000
		fci->no_separation = 1;
#endif
		if (zend_call_function(fci, fcc) != SUCCESS) {
			for (i = 0; i < params_count; i++) {
				zval_ptr_dtor(&func_params[i]);	
			}
			if (calldata) {
				php_error_docref(NULL, E_WARNING, "call to callback failed for request: '%s'", ZSTR_VAL(calldata->method));
			} else {
				php_error_docref(NULL, E_WARNING, "call to initial callback failed");
			}
			return 1;
		}
	} zend_catch {
		bailout = 1;
	} zend_end_try();

	zval_ptr_dtor(&retval_ptr);

	for (i = 0; i < params_count; i++) {
		zval_ptr_dtor(&func_params[i]);	
	}

    return bailout? 0 : 1;
} /* }}} */

int php_yar_concurrent_client_handle(yar_call_data_t *entry) /* {{{ */ {
	char *msg;
	yar_request_t *request;
	const yar_transport_t *factory;
	yar_transport_interface_t *transport;
	yar_transport_multi_interface_t *multi;
	int flags = 0;

	factory = php_yar_transport_get(ZEND_STRL("curl"));
	multi = factory->multi->init();

	while ((entry)) {
		if (entry->parameters == NULL) {
#if PHP_VERSION_ID < 70300
			ALLOC_HASHTABLE(entry->parameters);
			zend_hash_init(entry->parameters, 0, NULL, NULL, 0);
#else
			entry->parameters = (zend_array*)&zend_empty_array;
#endif
		} 

		transport = factory->init();
		if (entry->options && entry->options[YAR_OPT_PERSISTENT]) {
			flags |= YAR_PROTOCOL_PERSISTENT;
		}

		if (!(request = php_yar_request_instance(entry->method, entry->parameters, entry->options))) {
			transport->close(transport);
			factory->destroy(transport);
			return 0;
		}

		msg = (char*)entry->options;
		if (!transport->open(transport, entry->uri, flags, &msg)) {
			php_yar_client_trigger_error(1, YAR_ERR_TRANSPORT, msg);
			transport->close(transport);
			factory->destroy(transport);
			efree(msg);
			return 0;
		}

		DEBUG_C(ZEND_ULONG_FMT": call api '%s' at (%c)'%s' with '%d' parameters",
				request->id, ZSTR_VAL(request->method), (flags & YAR_PROTOCOL_PERSISTENT)? 'p' : 'r',
				entry->uri, zend_hash_num_elements(request->parameters));

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
		entry = entry->next;
	}

	if (!multi->exec(multi, php_yar_concurrent_client_callback)) {
		multi->close(multi);
		return 0;
	}

	multi->close(multi);
	return 1;
} /* }}} */

static void yar_client_object_free(zend_object *object) /* {{{ */ {
	yar_client_object *client = php_yar_client_fetch_object(object);

	if (client->parameters) {
		if (GC_DELREF(client->parameters) == 0) {
			GC_REMOVE_FROM_BUFFER(client->parameters);
			zend_array_destroy(client->parameters);
		}
	}

	if (client->uri) {
		zend_string_release(client->uri);
	}

	if (client->options) {
		php_yar_options_dtor(client->options);
	}

	if (client->properties) {
		if (GC_DELREF(client->properties) == 0) {
			GC_REMOVE_FROM_BUFFER(client->properties);
			zend_array_destroy(client->properties);
		}
	}

	zend_object_std_dtor(object);
}
/* }}} */

static HashTable *yar_client_get_properties(void *object) /* {{{ */ {
	zval rv;
	HashTable *ht;
#if PHP_VERSION_ID < 80000
	yar_client_object *client = php_yar_client_fetch_object(Z_OBJ_P((zval*)object));
#else
	yar_client_object *client = php_yar_client_fetch_object(((zend_object*)object));
#endif

	if (!client->properties) {
		ALLOC_HASHTABLE(client->properties);
		zend_hash_init(client->properties, 8, NULL, ZVAL_PTR_DTOR, 0);
	}

	ht = client->properties;

	ZVAL_LONG(&rv, client->protocol);
	zend_hash_str_update(ht, "_protocol", sizeof("_protocol") - 1, &rv);

	ZVAL_STR_COPY(&rv, client->uri);
	zend_hash_str_update(ht, "_uri", sizeof("_uri") - 1, &rv);

	if (client->options) {
		unsigned int i;
		zval zv;

		array_init(&rv);
		for (i = 0; i < YAR_OPT_MAX; i++) {
			if (client->options[i]) {
				if (php_yar_client_get_opt(client->options, i, &zv)) {
					zend_hash_index_update(Z_ARRVAL(rv), i, &zv);
				}
			}
		}
	} else {
		ZVAL_NULL(&rv);
	}

	zend_hash_str_update(ht, "_options", sizeof("_options") - 1, &rv);

	ZVAL_BOOL(&rv, client->running);
	zend_hash_str_update(ht, "_running", sizeof("_running") - 1, &rv);

	return ht;
}
/* }}} */

static HashTable *yar_client_get_gc(void *object, zval **table, int *n) /* {{{ */ {
#if PHP_VERSION_ID < 80000
	yar_client_object *client = php_yar_client_fetch_object(Z_OBJ_P((zval*)object));
#else
	yar_client_object *client = php_yar_client_fetch_object((zend_object*)object);
#endif

	*table = NULL;
	*n = 0;

	return client->parameters;
}
/* }}} */

static zend_object *yar_client_new(zend_class_entry *ce) /* {{{ */ {
	yar_client_object *client = emalloc(sizeof(yar_client_object) + zend_object_properties_size(ce));

	memset(client, 0, XtOffsetOf(yar_client_object, std));
	zend_object_std_init(&client->std, ce);

	if (ce->default_properties_count) {
		object_properties_init(&client->std, ce);
	}
	client->std.handlers = &yar_client_obj_handlers;

	return &client->std;
}
/* }}} */

/* {{{ proto Yar_Client::__construct($uri[, array $options = NULL]) */
PHP_METHOD(yar_client, __construct) {
	zend_string *url;
	zval *options = NULL;
	yar_client_object *client = Z_YARCLIENTOBJ_P(getThis());

    if (zend_parse_parameters_throw(ZEND_NUM_ARGS(), "S|a!", &url, &options) == FAILURE) {
        return;
    }

	if (strncasecmp(ZSTR_VAL(url), "http://", sizeof("http://") - 1) == 0) {
		client->protocol = YAR_CLIENT_PROTOCOL_HTTP;
	} else if (strncasecmp(ZSTR_VAL(url), "https://", sizeof("https://") - 1) == 0) {
		client->protocol = YAR_CLIENT_PROTOCOL_HTTP;
	} else if (strncasecmp(ZSTR_VAL(url), "tcp://", sizeof("tcp://") - 1) == 0) {
		client->protocol = YAR_CLIENT_PROTOCOL_TCP;
	} else if (strncasecmp(ZSTR_VAL(url), "unix://", sizeof("unix://") - 1) == 0) {
		client->protocol = YAR_CLIENT_PROTOCOL_UNIX;
	} else {
		php_yar_client_trigger_error(1, YAR_ERR_PROTOCOL, "unsupported protocol address %s", ZSTR_VAL(url));
		return;
	}

	client->uri = zend_string_copy(url);

	if (options) {
		zval *zv;
		zend_long h;
		ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARRVAL_P(options), h, zv) {
			if (!php_yar_client_validate_option(client->protocol, h)) {
				continue;
			}
			if (client->options == NULL) {
				client->options = ecalloc(YAR_OPT_MAX, sizeof(void*));
			}
            if (!php_yar_client_set_opt(client->options, h, zv)) {
				php_yar_client_trigger_error(1, YAR_ERR_EXCEPTION, "illegal option");
				return;
			}
		} ZEND_HASH_FOREACH_END();
	}
}
/* }}} */

/* {{{ proto Yar_Client::__call($method, $parameters = NULL) */
PHP_METHOD(yar_client, __call) {
	zval *params;
	zend_string *method;
	yar_client_object *client = Z_YARCLIENTOBJ_P(getThis());

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sa", &method, &params) == FAILURE) {
		return;
	}

	switch (client->protocol) {
		case YAR_CLIENT_PROTOCOL_TCP:
		case YAR_CLIENT_PROTOCOL_UNIX:
		case YAR_CLIENT_PROTOCOL_HTTP:
			if ((php_yar_client_handle(client, method, Z_ARRVAL_P(params), return_value))) {
				return;
			}
			break;
		default:
			php_error_docref(NULL, E_WARNING, "unsupported protocol %d", client->protocol);
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
	yar_client_object *client = Z_YARCLIENTOBJ_P(getThis());

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &type) == FAILURE) {
		return;
	}

	if (!php_yar_client_get_opt(client->options, type, return_value)) {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto Yar_Client::setOpt(int $type, mixed $value) */
PHP_METHOD(yar_client, setOpt) {
	long type;
	zval *value;
	yar_client_object *client = Z_YARCLIENTOBJ_P(getThis());

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "lz", &type, &value) == FAILURE) {
		return;
	}

	if (!php_yar_client_validate_option(client->protocol, type)) {
		RETURN_FALSE;
	}

	if (client->options == NULL) {
		client->options = ecalloc(YAR_OPT_MAX, sizeof(void*));
	}

	if (!php_yar_client_set_opt(client->options, type, value)) {
		RETURN_FALSE;
	}

	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/* {{{ proto Yar_Concurrent_Client::call($uri, $method, $parameters = NULL, $callback = NULL, $error_callback = NULL, $options = array()) */
PHP_METHOD(yar_concurrent_client, call) {
	zend_string *uri, *method;
	zend_fcall_info callback = {0}, ecallback = {0};
	zend_fcall_info_cache callbackc, ecallbackc;
	zval *parameters = NULL, *options = NULL;
	yar_call_data_t *entry;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|a!f!f!a!", &uri, &method,
				&parameters, &callback, &callbackc, &ecallback, &ecallbackc, &options) == FAILURE) {
		return;
	}

	if (UNEXPECTED(!ZSTR_LEN(uri))) {
		php_error_docref(NULL, E_WARNING, "first parameter is expected to be a valid rpc server uri");
		return;
	}

	if (UNEXPECTED(strncasecmp(ZSTR_VAL(uri), "http://", sizeof("http://") - 1) &&
		strncasecmp(ZSTR_VAL(uri), "https://", sizeof("https://") - 1))) {
		php_error_docref(NULL, E_WARNING, "only http protocol is supported in concurrent client for now");
		return;
	}

	if (UNEXPECTED(!ZSTR_LEN(method))) {
		php_error_docref(NULL, E_WARNING, "second parameter is expected to be a valid rpc api name");
		return;
	}

	if (UNEXPECTED(YAR_G(cctx).start)) {
        php_error_docref(NULL, E_WARNING, "concurrent client has already been started");
		RETURN_FALSE;
	}

	if (YAR_G(cctx).clist) {
		if (UNEXPECTED(((yar_call_data_t*)YAR_G(cctx).clist)->sequence == YAR_MAX_CALLS)) {
			php_error_docref(NULL, E_WARNING, "too many calls, maximum '%d' are allowed", YAR_MAX_CALLS);
			RETURN_FALSE;
		}
	}

	entry = ecalloc(1, sizeof(yar_call_data_t));

	entry->uri = zend_string_copy(uri);
	entry->method = zend_string_copy(method);

	if (callback.size) {
		memcpy(&entry->callback.fci, &callback, sizeof(zend_fcall_info));
		memcpy(&entry->callback.fcc, &callbackc, sizeof(zend_fcall_info_cache));
		Z_TRY_ADDREF(entry->callback.fci.function_name);
	}
	if (ecallback.size) {
		memcpy(&entry->ecallback.fci, &ecallback, sizeof(zend_fcall_info));
		memcpy(&entry->ecallback.fcc, &ecallbackc, sizeof(zend_fcall_info_cache));
		Z_TRY_ADDREF(entry->ecallback.fci.function_name);
	}

	if (parameters) {
		entry->parameters = zend_array_dup(Z_ARRVAL_P(parameters));
	}

	if (options) {
		zval *zv;
		zend_long h;

		entry->options = ecalloc(YAR_OPT_MAX, sizeof(void*));
		ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARRVAL_P(options), h, zv) {
            if (!php_yar_client_set_opt(entry->options, h, zv)) {
				php_yar_client_trigger_error(1, YAR_ERR_EXCEPTION, "illegal option");
				return;
			}
		} ZEND_HASH_FOREACH_END();
	}

	entry->next = YAR_G(cctx).clist;
	YAR_G(cctx).clist = entry;

	if (entry->next) {
		entry->sequence = entry->next->sequence + 1;
	} else {
		entry->sequence = 1;
	}

	RETURN_LONG(entry->sequence);
}
/* }}} */

/* {{{ proto Yar_Concurrent_Client::reset(void) */
PHP_METHOD(yar_concurrent_client, reset) {
	if (YAR_G(cctx).start) {
        php_error_docref(NULL, E_WARNING, "cannot reset while client is running");
		RETURN_FALSE;
	}
	php_yar_calllist_dtor();
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto Yar_Concurrent_Client::loop($callback = NULL, $error_callback = NULL) */
PHP_METHOD(yar_concurrent_client, loop) {
	int ret;
	zval *options;
	zend_fcall_info callback = {0}, ecallback = {0};
	zend_fcall_info_cache callbackc, ecallbackc;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|f!f!a!", &callback, &callbackc, &ecallback, &ecallbackc, &options) == FAILURE) {
		return;
	}

	if (UNEXPECTED(YAR_G(cctx).start)) {
        php_error_docref(NULL, E_WARNING, "concurrent client has already been started");
		RETURN_FALSE;
	}

	if (UNEXPECTED(YAR_G(cctx).clist == NULL)) {
		RETURN_TRUE;
	}

	if (options) {
		void *coptions[YAR_OPT_MAX];
		yar_call_data_t *entry = (yar_call_data_t*)YAR_G(cctx).clist;
		ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARRVAL_P(options), h, zv) {
            if (!php_yar_client_set_opt(coptions, h, zv)) {
				php_yar_client_trigger_error(1, YAR_ERR_EXCEPTION, "illegal option");
				RETURN_FALSE;
			}
		} ZEND_HASH_FOREACH_END();

		while (entry) {
			if (entry->options) {
				int i = 0;
				for (i; i < YAR_OPT_MAX; i++) {
				}
			}
		}
	}

	if (callback.size) {
		memcpy(&YAR_G(cctx).callback.fci, &callback, sizeof(zend_fcall_info));
		memcpy(&YAR_G(cctx).callback.fcc, &callbackc, sizeof(zend_fcall_info_cache));
	} else {
		YAR_G(cctx).callback.fci.size = 0;
	}
	if (ecallback.size) {
		memcpy(&YAR_G(cctx).ecallback.fci, &ecallback, sizeof(zend_fcall_info));
		memcpy(&YAR_G(cctx).ecallback.fcc, &ecallbackc, sizeof(zend_fcall_info_cache));
	} else {
		YAR_G(cctx).ecallback.fci.size = 0;
	}

	YAR_G(cctx).start = 1;
	ret = php_yar_concurrent_client_handle((yar_call_data_t*)YAR_G(cctx).clist);
	YAR_G(cctx).start = 0;

	php_yar_calllist_dtor();

	RETURN_BOOL(ret);
}
/* }}} */

/* {{{ yar_client_methods */
zend_function_entry yar_client_methods[] = {
	PHP_ME(yar_client, __construct, arginfo_class_Yar_Client___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)
	PHP_ME(yar_client, call, arginfo_class_Yar_Client_call, ZEND_ACC_PUBLIC)
	PHP_ME(yar_client, __call, arginfo_class_Yar_Client___call, ZEND_ACC_PUBLIC)
	PHP_ME(yar_client, getOpt, arginfo_class_Yar_Client_getOpt, ZEND_ACC_PUBLIC)
	PHP_ME(yar_client, setOpt, arginfo_class_Yar_Client_setOpt, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

/* {{{ yar_concurrent_client_methods */
zend_function_entry yar_concurrent_client_methods[] = {
	PHP_ME(yar_concurrent_client, call, arginfo_class_Yar_Concurrent_Client_call, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(yar_concurrent_client, loop, arginfo_class_Yar_Concurrent_Client_loop, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(yar_concurrent_client, reset, arginfo_class_Yar_Concurrent_Client_reset, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_FE_END
};
/* }}} */

YAR_STARTUP_FUNCTION(client) /* {{{ */ {
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "Yar_Client", yar_client_methods);
    yar_client_ce = zend_register_internal_class(&ce);
	yar_client_ce->create_object = yar_client_new;

	memcpy(&yar_client_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	yar_client_obj_handlers.offset = XtOffsetOf(yar_client_object, std);
	yar_client_obj_handlers.free_obj = yar_client_object_free;
	yar_client_obj_handlers.get_properties = (zend_object_get_properties_t)yar_client_get_properties;
	yar_client_obj_handlers.get_gc = (zend_object_get_gc_t)yar_client_get_gc;
	yar_client_obj_handlers.clone_obj = NULL;

	INIT_CLASS_ENTRY(ce, "Yar_Concurrent_Client", yar_concurrent_client_methods);
    yar_concurrent_client_ce = zend_register_internal_class(&ce);	

	REGISTER_LONG_CONSTANT("YAR_CLIENT_PROTOCOL_HTTP", YAR_CLIENT_PROTOCOL_HTTP, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("YAR_CLIENT_PROTOCOL_TCP", YAR_CLIENT_PROTOCOL_TCP, CONST_PERSISTENT | CONST_CS);
	REGISTER_LONG_CONSTANT("YAR_CLIENT_PROTOCOL_UNIX", YAR_CLIENT_PROTOCOL_UNIX, CONST_PERSISTENT | CONST_CS);

    return SUCCESS;
}
/* }}} */
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
