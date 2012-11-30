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
#include "SAPI.h"
#include "Zend/zend_exceptions.h"
#include "ext/standard/php_lcg.h" /* for php_combined_lcg’ */
#include "ext/standard/php_rand.h" /* for php_mt_rand */
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
ZEND_BEGIN_ARG_INFO_EX(arginfo_client_setopt, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
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
		} else if (status == YAR_ERR_EXCEPTION) {
			if (zend_hash_find(ht, ZEND_STRS("e"), (void **)&ppzval) == SUCCESS) {
				if (throw_exception) {
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
					zend_throw_exception_object(ex TSRMLS_CC);
				} else {
					zval **msg, **code;
					if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("message"), (void **)&msg) == SUCCESS
							&& zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("code"), (void **)&code) == SUCCESS) {
						convert_to_string_ex(msg);
						convert_to_long_ex(code);
						php_yar_client_trigger_error(0 TSRMLS_CC, Z_LVAL_PP(code), "server threw an exception with message `%s`", Z_STRVAL_PP(msg));
					}
				}
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

static int php_yar_client_prepare(yar_transport_interface_t *transport, char *uri, long ulen, char *method, long mlen, zval *params, zval *options TSRMLS_DC) /* {{{ */ {
	zval request;
	char *payload, *err_msg;
	char *packager_name = NULL;
	size_t payload_len;
	yar_header_t header = {0};
	long request_id;
	php_url *url = NULL;
   
	if (!BG(mt_rand_is_seeded)) {
		php_mt_srand(GENERATE_SEED() TSRMLS_CC);
	}
    request_id = (long)php_mt_rand(TSRMLS_C);

	if (options && IS_ARRAY == Z_TYPE_P(options)) {
		zval **ppzval;
		if (zend_hash_index_find(Z_ARRVAL_P(options), YAR_CLIENT_OPT_PACKAGER, (void **)&ppzval) == SUCCESS
				&& IS_STRING == Z_TYPE_PP(ppzval)) {
			packager_name = Z_STRVAL_PP(ppzval);
			if (YAR_G(debug)) {
				php_yar_debug_client("%ld: set packager to %s", request_id, packager_name);
			}
		}
		if (zend_hash_index_find(Z_ARRVAL_P(options), YAR_CLIENT_OPT_TIMEOUT, (void **)&ppzval) == SUCCESS) {
			convert_to_long_ex(ppzval);
			transport->setopt(transport, YAR_CLIENT_OPT_TIMEOUT, (long *)&Z_LVAL_PP(ppzval), NULL TSRMLS_CC);
			if (YAR_G(debug)) {
				php_yar_debug_client("%ld: set transport timeout to %ls", request_id, Z_LVAL_PP(ppzval));
			}
		}
	}

	INIT_ZVAL(request);
	array_init(&request);

	add_assoc_long_ex(&request, ZEND_STRS("i"), request_id);
	add_assoc_stringl_ex(&request, ZEND_STRS("m"), method, mlen, 1);
	Z_ADDREF_P(params);
    add_assoc_zval_ex(&request, ZEND_STRS("p"), params);


	if (YAR_G(debug)) {
		php_yar_debug_client("%ld: call api '%s' at '%s' with '%d parameters'",
				request_id, method, uri, zend_hash_num_elements(Z_ARRVAL_P(params)));
	}

	if (!(payload_len = php_yar_packager_pack(packager_name, &request, &payload, &err_msg TSRMLS_CC))) {
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

	if (!strncasecmp(uri, ZEND_STRL("http")) && !(url = php_url_parse(uri))) {
		efree(payload);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "malformed uri: '%s'", uri);
		return 0;
	}

	if (!transport->open(transport, uri, ulen, url? url->host : NULL, 0, &err_msg TSRMLS_CC)) {
		php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_TRANSPORT, "%s", err_msg);
		efree(err_msg);
		return 0;
	}

	php_yar_protocol_render(&header, request_id, url? url->user : NULL, url? url->pass : NULL, payload_len, 0 TSRMLS_CC);

	if (url) {
		php_url_free(url);
	}

    transport->send(transport, (char *)&header, sizeof(yar_header_t) TSRMLS_CC);
    transport->send(transport, payload, payload_len TSRMLS_CC);
	efree(payload);

	return 1;
} /* }}} */

static zval * php_yar_client_handle(int protocol, zval *client, char *method, long mlen, zval *params TSRMLS_DC) /* {{{ */ {
	size_t ret_len;
	char *ret, *err_msg;
	uint err_code;
	yar_transport_t *factory;
	yar_transport_interface_t *transport;
	zval *uri, *response = NULL, *options;

	uri = zend_read_property(yar_client_ce, client, ZEND_STRL("_uri"), 0 TSRMLS_CC);

	if (protocol == YAR_CLIENT_PROTOCOL_HTTP) {
		factory = php_yar_transport_get(ZEND_STRL("curl") TSRMLS_CC);
	} else if (protocol == YAR_CLIENT_PROTOCOL_TCP || protocol == YAR_CLIENT_PROTOCOL_UNIX) {
		factory = php_yar_transport_get(ZEND_STRL("sock") TSRMLS_CC);
	}

	transport = factory->init(TSRMLS_C);

	options = zend_read_property(yar_client_ce, client, ZEND_STRL("_options"), 0 TSRMLS_CC);

	if (IS_ARRAY != Z_TYPE_P(options)) {
		options = NULL;
	}

 	if (!php_yar_client_prepare(transport, Z_STRVAL_P(uri), Z_STRLEN_P(uri), method, mlen, params, options TSRMLS_CC)) {
		transport->close(transport TSRMLS_CC);
		factory->destroy(transport TSRMLS_CC);
        return NULL;
	}

	if (transport->exec(transport, &ret, &ret_len, &err_code, &err_msg TSRMLS_CC)) {
		if (ret_len) {
			response = php_yar_client_parse_response(ret, ret_len, 1 TSRMLS_CC);
			efree(ret);
		} else {
			php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_PROTOCOL, "%s", "server responsed empty response");
			if (ret) {
				efree(ret);
			}
		}
	} else {
		php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_TRANSPORT, err_msg);
		efree(err_msg);
	}

	transport->close(transport TSRMLS_CC);
	factory->destroy(transport TSRMLS_CC);
	return response;
} /* }}} */

static int php_yar_client_set_opt(zval *client, long type, zval *value TSRMLS_DC) /* {{{ */ {
	zend_bool verified = 0;
	switch (type) {
		case YAR_CLIENT_OPT_PACKAGER:
		{
			 verified = 1;
             if (IS_STRING != Z_TYPE_P(value)) {
				 php_error_docref(NULL TSRMLS_CC, E_WARNING, "expects a string packager name");
				 return 0;
			 }
		}
		case YAR_CLIENT_OPT_TIMEOUT:
		case YAR_CLIENT_OPT_CONNECT_TIMEOUT:
		{
			zval *options;
			if (!verified) {
				verified = 1;
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

int php_yar_concurrent_client_callback(zval *calldata, int status, int err, char *ret, size_t len TSRMLS_DC) /* {{{ */ {
	zval **method, **uri, **sequence;
	zval *response, *code, *retval_ptr = NULL;
	zval *callinfo, *callback, ***func_params;
	zend_bool bailout = 0;
	uint params_count;

	if (calldata) {
		/* data callback */
		if (status == YAR_ERR_OKEY) {
			zval **ppzval;
			if (zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("c"), (void **)&ppzval) == SUCCESS) {
				callback = *ppzval;
			} else {
				callback = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callback"), 0 TSRMLS_CC);
			}
			params_count = 2;
		} else {
			zval **ppzval;
			if (zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("e"), (void **)&ppzval) == SUCCESS) {
				callback = *ppzval;
			} else {
				callback = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_error_callback"), 0 TSRMLS_CC);
			}
			params_count = 3;
		}

		if (ZVAL_IS_NULL(callback)) {
			if (status != YAR_ERR_OKEY) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "[%d]:%s", status, ret);
			} else if (len) {
				response = php_yar_client_parse_response(ret, len, 0 TSRMLS_CC);
				efree(ret);
				zend_print_zval(response, 1);
				zval_ptr_dtor(&response);
			}
			return 1;
		}

		if (status == YAR_ERR_OKEY) {
			if (len) {
				response = php_yar_client_parse_response(ret, len, 0 TSRMLS_CC);
				efree(ret);
			} else {
				php_yar_client_trigger_error(0 TSRMLS_CC, YAR_ERR_REQUEST, "%s", "server responsed empty response");
				if (ret) {
					efree(ret);
				}
				return 1;
			}
		} else {
			MAKE_STD_ZVAL(code);
			ZVAL_LONG(code, err);
			MAKE_STD_ZVAL(response);
			ZVAL_STRINGL(response, ret, len, 1);
		}

		zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("u"), (void **)&uri);
		zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("m"), (void **)&method);
		zend_hash_find(Z_ARRVAL_P(calldata), ZEND_STRS("i"), (void **)&sequence);

		MAKE_STD_ZVAL(callinfo);
		array_init(callinfo);
		Z_ADDREF_P(*uri);
		Z_ADDREF_P(*method);
		Z_ADDREF_P(*sequence);

		zend_hash_update(Z_ARRVAL_P(callinfo), "uri", sizeof("uri"), (void **)uri, sizeof(zval *), NULL);
		zend_hash_update(Z_ARRVAL_P(callinfo), "method", sizeof("method"), (void **)method, sizeof(zval *), NULL);
		zend_hash_update(Z_ARRVAL_P(callinfo), "sequence", sizeof("sequence"), (void **)sequence, sizeof(zval *), NULL);
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
		func_params[1] = &response;
		func_params[2] = &callinfo;
	} else if (calldata) {
		func_params[0] = &response;
		func_params[1] = &callinfo;
	} else {
		MAKE_STD_ZVAL(response);
		MAKE_STD_ZVAL(callinfo);
		ZVAL_NULL(response);
		ZVAL_NULL(callinfo);
		func_params[0] = &response;
		func_params[1] = &callinfo;
	}

	zend_try {
		if (call_user_function_ex(EG(function_table), NULL, callback,
					&retval_ptr, params_count, func_params, 0, NULL TSRMLS_CC) != SUCCESS) {
			if (status) {
				zval_ptr_dtor(&code);
			}
			zval_ptr_dtor(&response);
			zval_ptr_dtor(&callinfo);
			efree(func_params);
			if (calldata) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "call to callback failed for request: %s", Z_STRVAL_PP(method));
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "call to initial callback failed");
			}
			return 1;
		}
	} zend_catch {
		bailout = 1;
	} zend_end_try();

	zval_ptr_dtor(&response);
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
	zval **entry;
	char *dummy;
	ulong sequence;
	zval **uri, **method, **parameters, **options;
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

		if (zend_hash_find(Z_ARRVAL_PP(entry), ZEND_STRS("o"), (void **)&options) == FAILURE
				|| IS_ARRAY !=  Z_TYPE_PP(options)) {
			options = NULL;
		}

		if (!php_yar_client_prepare(transport, Z_STRVAL_PP(uri), Z_STRLEN_PP(uri),
					Z_STRVAL_PP(method), Z_STRLEN_PP(method), *parameters, options? *options : NULL TSRMLS_CC)) {
			transport->close(transport TSRMLS_CC);
			multi->close(multi TSRMLS_CC);
			return 0;
		}
		add_assoc_long_ex(*entry, ZEND_STRS("i"), sequence);
		transport->calldata(transport, *entry TSRMLS_CC);
		multi->add(multi, transport TSRMLS_CC);
	}

	if (!multi->exec(multi, php_yar_concurrent_client_callback TSRMLS_CC)) {
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

	if (strncasecmp(url, ZEND_STRL("http://")) == 0 || strncasecmp(url, ZEND_STRL("https://")) == 0) {
	} else if (strncasecmp(url, ZEND_STRL("tcp://")) == 0) {
		zend_update_property_long(yar_client_ce, getThis(), ZEND_STRL("_protocol"), YAR_CLIENT_PROTOCOL_TCP);
	} else if (strncasecmp(url, ZEND_STRL("unix://")) == 0) {
		zend_update_property_long(yar_client_ce, getThis(), ZEND_STRL("_protocol"), YAR_CLIENT_PROTOCOL_UNIX);
	} else {
		php_yar_client_trigger_error(1 TSRMLS_CC, YAR_ERR_PROTOCOL, "Unsupported protocol address %s", url);
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

	/* we only support HTTP now */
	switch (Z_LVAL_P(protocol)) {
		case YAR_CLIENT_PROTOCOL_TCP:
		case YAR_CLIENT_PROTOCOL_UNIX:
		case YAR_CLIENT_PROTOCOL_HTTP:
			if ((ret = php_yar_client_handle(Z_LVAL_P(protocol), getThis(), method, mlen, params TSRMLS_CC))) {
				RETVAL_ZVAL(ret, 0, 0);
				efree(ret);
				return;
			}
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unsupported protocol %ld", Z_LVAL_P(protocol));
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
	zval *callstack, *item;
	zval *error_callback = NULL, *callback = NULL, *parameters = NULL, *options = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a!z!za",
				&uri, &ulen, &method, &mlen, &parameters, &callback, &error_callback, &options) == FAILURE) {
		return;
	}

	if (!ulen) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "first parameter is expected to be a valid rpc server uri");
		return;
	}

	if (strncasecmp(uri, ZEND_STRL("http://")) && strncasecmp(uri, ZEND_STRL("https://"))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "only http protocol is supported in concurrent client");
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

	MAKE_STD_ZVAL(item);
	array_init(item);

	add_assoc_stringl_ex(item, ZEND_STRS("u"), uri, ulen, 1);
	add_assoc_stringl_ex(item, ZEND_STRS("m"), method, mlen, 1);
	if (callback && !ZVAL_IS_NULL(callback)) {
		Z_ADDREF_P(callback);
		add_assoc_zval_ex(item, ZEND_STRS("c"), callback);
	}
	if (error_callback && !ZVAL_IS_NULL(error_callback)) {
		Z_ADDREF_P(error_callback);
		add_assoc_zval_ex(item, ZEND_STRS("e"), error_callback);
	}
	if (parameters && IS_ARRAY == Z_TYPE_P(parameters)) {
		Z_ADDREF_P(parameters);
		add_assoc_zval_ex(item, ZEND_STRS("p"), parameters);
	}
	if (options && IS_ARRAY == Z_TYPE_P(options)) {
		Z_ADDREF_P(options);
		add_assoc_zval_ex(item, ZEND_STRS("o"), options);
	}

	callstack = zend_read_static_property(yar_concurrent_client_ce, ZEND_STRL("_callstack"), 0 TSRMLS_CC);
	if (ZVAL_IS_NULL(callstack)) {
		MAKE_STD_ZVAL(callstack);
		array_init(callstack);
		zend_update_static_property(yar_concurrent_client_ce, ZEND_STRL("_callstack"), callstack TSRMLS_CC);
		zval_ptr_dtor(&callstack);
	}
	sequence = zend_hash_next_free_element(Z_ARRVAL_P(callstack));
	add_next_index_zval(callstack, item);

	RETURN_LONG(sequence);
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
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Concurrent client has already be started");
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

	REGISTER_LONG_CONSTANT("YAR_CLIENT_OPT_PACKAGER", YAR_CLIENT_OPT_PACKAGER, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_CLIENT_OPT_TIMEOUT", YAR_CLIENT_OPT_TIMEOUT, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_CLIENT_OPT_CONNECT_TIMEOUT", YAR_CLIENT_OPT_CONNECT_TIMEOUT, CONST_CS|CONST_PERSISTENT);

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
