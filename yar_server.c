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
#include "php_yar.h"
#include "yar_exception.h"
#include "yar_packager.h"
#include "yar_server.h"
#include "yar_request.h"
#include "yar_response.h"
#include "yar_protocol.h"

zend_class_entry *yar_server_ce;

/* {{{ ARG_INFO */
ZEND_BEGIN_ARG_INFO_EX(arginfo_service___construct, 0, 0, 1)
	ZEND_ARG_INFO(0, obj)
	ZEND_ARG_INFO(0, protocol)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_service_set_packager, 0, 0, 1)
	ZEND_ARG_INFO(0, protocol)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_service_void, 0, 0, 1)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ HTML Markups for service info */
#define HTML_MARKUP_HEADER  \
	"<!DOCTYPE html>\n" \
	"<html>\n" \
	" <head>\n" \
	"  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n" \
	"  <title>%s - Yar Server</title>\n"

#define HTML_MARKUP_CSS \
	"  <style>\n" \
	"   body { margin: 0; font:14px/20px Verdana, Arial, sans-serif; color: #333; background: #f8f8f8;}\n" \
	"   h1, h2, pre { margin: 0; padding: 0;}\n" \
	"   h1 { font:bold 28px Verdana,Arial; background:#99c; padding: 12px 5px; border-bottom: 4px solid #669; box-shadow: 0 1px 4px #bbb; color: #222;}\n" \
	"   h2 { font:normal 20px/22px Georgia, Times, \"Times New Roman\", serif; padding: 5px 0 8px; margin: 20px 10px 0; border-bottom: 1px solid #ddd; cursor:pointer;}\n" \
	"   p, dd { color: #555; }\n" \
	"   .api-info { padding: 10px 0; margin-left: 20px; }\n" \
	"   .api-block { margin-left: 40px;}\n" \
	"   h2 u { font-size:20px;text-decoration:none;padding:10px; }\n" \
	"  </style>\n"

#define HTML_MARKUP_SCRIPT  \
	"  <script>\n" \
	"   function _t(elem) {\n" \
	"    var block = elem.nextSibling;\n" \
	"    var info = elem.getElementsByTagName('u')[0];\n" \
	"    while (block) {\n" \
	"     if ( block.nodeType == 1 && block.className.indexOf('api-block') > -1 ) {\n" \
	"      break;\n" \
	"     }\n" \
	"     block = block.nextSibling;\n" \
	"    }\n" \
	"    var isHidden = block.style.display == 'none';\n" \
	"    block.style.display = isHidden ? '' : 'none';\n" \
	"    info.innerHTML = isHidden ? '+'  : '-';\n" \
	"   }\n" \
	"  </script>\n" \
	" </head>\n" \
	" <body>\n" \
	" <!-- powered by yar-" YAR_VERSION " -->\n"

#define HTML_MARKUP_TITLE \
	" <h1>Yar Server: %s</h1>"

#define HTML_MARKUP_ENTRY \
	" <h2 onclick=\"_t(this)\"><u>-</u>%s</h2>\n" \
	" <div class=\"api-block\" style=\"display:none\">\n" \
    " <pre style=\"white-space:pre-line\">%s</pre>\n" \
	" </div>\n" 

#define HTML_MARKUP_FOOTER  \
      "</body>\n" \
     "</html>"
/* }}} */

static char * php_yar_get_function_declaration(zend_function *fptr TSRMLS_DC) /* {{{ */ {
	char *offset, *buf;
	zend_uint length = 1024;

#define REALLOC_BUF_IF_EXCEED(buf, offset, length, size) \
	if (offset - buf + size >= length) { 	\
		length += size + 1; 				\
		buf = erealloc(buf, length); 		\
	}

	if (!(fptr->common.fn_flags & ZEND_ACC_PUBLIC)) {
		return NULL;
	}

	offset = buf = (char *)emalloc(length * sizeof(char));

#if PHP_API_VERSION > 20090626
	if (fptr->op_array.fn_flags & ZEND_ACC_RETURN_REFERENCE) {
		*(offset++) = '&';
		*(offset++) = ' ';
	}
#endif

	if (fptr->common.scope) {
		memcpy(offset, fptr->common.scope->name, fptr->common.scope->name_length);
		offset += fptr->common.scope->name_length;
		*(offset++) = ':';
		*(offset++) = ':';
	}

	{
		size_t name_len = strlen(fptr->common.function_name);
		REALLOC_BUF_IF_EXCEED(buf, offset, length, name_len);
		memcpy(offset, fptr->common.function_name, name_len);
		offset += name_len;
	}

	*(offset++) = '(';
	if (fptr->common.arg_info) {
		zend_uint i, required;
		zend_arg_info *arg_info = fptr->common.arg_info;

		required = fptr->common.required_num_args;
		for (i = 0; i < fptr->common.num_args;) {
			if (arg_info->class_name) {
				const char *class_name;
				zend_uint class_name_len;
				if (!strcasecmp(arg_info->class_name, "self") && fptr->common.scope ) {
					class_name = fptr->common.scope->name;
					class_name_len = fptr->common.scope->name_length;
				} else if (!strcasecmp(arg_info->class_name, "parent") && fptr->common.scope->parent) {
					class_name = fptr->common.scope->parent->name;
					class_name_len = fptr->common.scope->parent->name_length;
				} else {
					class_name = arg_info->class_name;
					class_name_len = arg_info->class_name_len;
				}
				REALLOC_BUF_IF_EXCEED(buf, offset, length, class_name_len);
				memcpy(offset, class_name, class_name_len);
				offset += class_name_len;
				*(offset++) = ' ';
#if PHP_API_VERSION > 20090626
			} else if (arg_info->type_hint) {
				zend_uint type_name_len;
				char *type_name = zend_get_type_by_const(arg_info->type_hint);
				type_name_len = strlen(type_name);
				REALLOC_BUF_IF_EXCEED(buf, offset, length, type_name_len);
				memcpy(offset, type_name, type_name_len);
				offset += type_name_len;
				*(offset++) = ' ';
#else
			} else if (arg_info->array_type_hint) {
				REALLOC_BUF_IF_EXCEED(buf, offset, length, 5);
				memcpy(offset, "array", 5);
				offset += 5;
				*(offset++) = ' ';
#endif
			}

			if (arg_info->pass_by_reference) {
				*(offset++) = '&';
			}
			*(offset++) = '$';

			if (arg_info->name) {
				REALLOC_BUF_IF_EXCEED(buf, offset, length, arg_info->name_len);
				memcpy(offset, arg_info->name, arg_info->name_len);
				offset += arg_info->name_len;
			} else {
				zend_uint idx = i;
				memcpy(offset, "param", 5);
				offset += 5;
				do {
					*(offset++) = (char) (idx % 10) + '0';
					idx /= 10;
				} while (idx > 0);
			}
			if (i >= required) {
				*(offset++) = ' ';
				*(offset++) = '=';
				*(offset++) = ' ';
				if (fptr->type == ZEND_USER_FUNCTION) {
					zend_op *precv = NULL;
					{
						zend_uint idx  = i;
						zend_op *op = ((zend_op_array *)fptr)->opcodes;
						zend_op *end = op + ((zend_op_array *)fptr)->last;

						++idx;
						while (op < end) {
							if ((op->opcode == ZEND_RECV || op->opcode == ZEND_RECV_INIT)
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4))
							        && op->op1.u.constant.value.lval == (long)idx
#else
									&& op->op1.num == (long)idx
#endif
									) {
								precv = op;
							}
							++op;
						}
					}
					if (precv && precv->opcode == ZEND_RECV_INIT
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4))
						   	&& precv->op2.op_type != IS_UNUSED
#else
						   	&& precv->op2_type != IS_UNUSED
#endif
							) {
						zval *zv, zv_copy;
						int use_copy;
						ALLOC_ZVAL(zv);
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4))
						*zv = precv->op2.u.constant;
#else
						*zv = *precv->op2.zv;
#endif
						zval_copy_ctor(zv);
						INIT_PZVAL(zv);
						zval_update_constant_ex(&zv, (void*)1, fptr->common.scope TSRMLS_CC);
						if (Z_TYPE_P(zv) == IS_BOOL) {
							if (Z_LVAL_P(zv)) {
								memcpy(offset, "true", 4);
								offset += 4;
							} else {
								memcpy(offset, "false", 5);
								offset += 5;
							}
						} else if (Z_TYPE_P(zv) == IS_NULL) {
							memcpy(offset, "NULL", 4);
							offset += 4;
						} else if (Z_TYPE_P(zv) == IS_STRING) {
							*(offset++) = '\'';
							REALLOC_BUF_IF_EXCEED(buf, offset, length, MIN(Z_STRLEN_P(zv), 10));
							memcpy(offset, Z_STRVAL_P(zv), MIN(Z_STRLEN_P(zv), 10));
							offset += MIN(Z_STRLEN_P(zv), 10);
							if (Z_STRLEN_P(zv) > 10) {
								*(offset++) = '.';
								*(offset++) = '.';
								*(offset++) = '.';
							}
							*(offset++) = '\'';
						} else if (Z_TYPE_P(zv) == IS_ARRAY) {
							memcpy(offset, "Array", 5);
							offset += 5;
						} else {
							zend_make_printable_zval(zv, &zv_copy, &use_copy);
							REALLOC_BUF_IF_EXCEED(buf, offset, length, Z_STRLEN(zv_copy));
							memcpy(offset, Z_STRVAL(zv_copy), Z_STRLEN(zv_copy));
							offset += Z_STRLEN(zv_copy);
							if (use_copy) {
								zval_dtor(&zv_copy);
							}
						}
						zval_ptr_dtor(&zv);
					}
				} else {
					memcpy(offset, "NULL", 4);
					offset += 4;
				}
			}

			if (++i < fptr->common.num_args) {
				*(offset++) = ',';
				*(offset++) = ' ';
			}
			arg_info++;
			REALLOC_BUF_IF_EXCEED(buf, offset, length, 32);
		}
	}
	*(offset++) = ')';
	*offset = '\0';

	return buf;
}
/* }}} */

static int php_yar_print_info(void *ptr, void *argument TSRMLS_DC) /* {{{ */ {
    zend_function *f = ptr;
    zend_class_entry *ce = argument;

    if (f->common.fn_flags & ZEND_ACC_PUBLIC 
			&& f->common.function_name && *(f->common.function_name) != '_') {
        char *prototype = NULL;
		if ((prototype = php_yar_get_function_declaration(f TSRMLS_CC))) {
			char *buf, *doc_comment = NULL;
			uint buf_len;
			if (f->type == ZEND_USER_FUNCTION) {
				doc_comment = (char *)f->op_array.doc_comment;
			}
			buf_len = spprintf(&buf, 0, HTML_MARKUP_ENTRY, prototype, doc_comment? doc_comment : "");
			efree(prototype);
			PHPWRITE(buf, strlen(buf));
            efree(buf);
		}
    }

	return ZEND_HASH_APPLY_KEEP;
} /* }}} */

static void php_yar_server_response_header(size_t content_lenth, void *packager_info TSRMLS_DC) /* {{{ */ {
	sapi_header_line ctr = {0};
	char header_line[512];

	ctr.line_len = snprintf(header_line, sizeof(header_line), "Content-Length: %ld", content_lenth);
	ctr.line = header_line;
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);

	ctr.line_len = snprintf(header_line, sizeof(header_line), "Content-Type: text/plain");
	ctr.line = header_line;
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);

	ctr.line_len = snprintf(header_line, sizeof(header_line), "Cache-Control: no-cache");
	ctr.line = header_line;
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);

	ctr.line_len = snprintf(header_line, sizeof(header_line), "Connection: close");
	ctr.line = header_line;
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);

	php_header(TSRMLS_C);

	return;
} /* }}} */

static void php_yar_server_response(yar_request_t *request, yar_response_t *response TSRMLS_DC) /* {{{ */ {
	zval ret, *tmp;
	char *payload, *err_msg;
	size_t payload_len;
	yar_header_t header = {0};

	INIT_ZVAL(ret);
	array_init(&ret);

	add_assoc_long_ex(&ret, ZEND_STRS("i"), response->id);
	add_assoc_long_ex(&ret, ZEND_STRS("s"), response->status);
	if (response->out) {
		add_assoc_string_ex(&ret, ZEND_STRS("o"), response->out, 1);
	}
	if (response->retval) {
		Z_ADDREF_P(response->retval);
		add_assoc_zval_ex(&ret, ZEND_STRS("r"), response->retval);
	}
	if (response->err) {
		Z_ADDREF_P(response->err);
		add_assoc_zval_ex(&ret, ZEND_STRS("e"), response->err);
	}

    if (!(payload_len = php_yar_packager_pack(NULL, &ret, &payload, &err_msg TSRMLS_CC))) {
		zval_dtor(&ret);
		php_yar_error(response, YAR_ERR_PACKAGER TSRMLS_CC, "%s", err_msg);
		efree(err_msg);
		return;
	}
	zval_dtor(&ret);

	php_yar_protocol_render(&header, request->id, "PHP Yar Server", NULL, payload_len, 0 TSRMLS_CC);
	if (YAR_G(debug)) {
		php_yar_debug_server("%ld: server response: packager '%s', len '%ld', content '%s'",
				request->id, payload, payload_len - 8, payload + 8);
	}

	php_yar_server_response_header(sizeof(yar_header_t) + payload_len, payload TSRMLS_CC);
	PHPWRITE((char *)&header, sizeof(yar_header_t));
	if (payload_len) {
		PHPWRITE(payload, payload_len);
		efree(payload);
		return;
	}

	return;
} /* }}} */

static void php_yar_server_handle(zval *obj TSRMLS_DC) /* {{{ */ {
	char *payload, *err_msg, method[256];
	size_t payload_len;
	zend_bool bailout = 0;
	zval *post_data = NULL, output;
	zend_class_entry *ce;
	yar_response_t *response;
	yar_request_t  *request = NULL;
	yar_header_t *header;

	response = php_yar_response_instance(TSRMLS_C);
	if (!SG(request_info).raw_post_data) {
		goto response_no_output;
	}

	payload = SG(request_info).raw_post_data;
	payload_len = SG(request_info).raw_post_data_length;
	if (!(header = php_yar_protocol_parse(&payload, &payload_len, &err_msg TSRMLS_CC))) {
		php_yar_error(response, YAR_ERR_PACKAGER TSRMLS_CC, err_msg);
		if (YAR_G(debug)) {
			php_yar_debug_server("0: an malformed request '%s'", payload);
		}
		efree(err_msg);
		goto response_no_output;
	}

	if (YAR_G(debug)) {
		php_yar_debug_server("%ld: accpect rpc request form '%s'",
				header->id, header->provider? (char *)header->provider : "Yar PHP " YAR_VERSION);
	}

	if (!(post_data = php_yar_packager_unpack(payload, payload_len, &err_msg TSRMLS_CC))) {
        php_yar_error(response, YAR_ERR_PACKAGER TSRMLS_CC, err_msg);
		efree(err_msg);
		goto response_no_output;
	}

	request = php_yar_request_instance(post_data TSRMLS_CC);
	zval_ptr_dtor(&post_data);
	ce = Z_OBJCE_P(obj);

	if (!php_yar_request_valid(request, response TSRMLS_CC)) {
		goto response_no_output;
	}

#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4))
	if (php_start_ob_buffer(NULL, 0, 0 TSRMLS_CC) != SUCCESS) {
#else
	if (php_output_start_user(NULL, 0, PHP_OUTPUT_HANDLER_STDFLAGS TSRMLS_CC) == FAILURE) {
#endif
		php_yar_error(response, YAR_ERR_OUTPUT TSRMLS_CC, "start output buffer failed");
		goto response_no_output;
	}

	ce = Z_OBJCE_P(obj);
	zend_str_tolower_copy(method, Z_STRVAL_P(request->method), sizeof(method) - 1);
	if (!zend_hash_exists(&ce->function_table, method, strlen(method) + 1)) {
		php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "call to undefined api %s::%s()", ce->name, Z_STRVAL_P(request->method));
		goto response;
	}

	zend_try {
		uint count;
		zval ***func_params;
		zval *retval_ptr = NULL;
		HashTable *func_params_ht;

		INIT_ZVAL(output);

		if (zend_hash_exists(&ce->function_table, ZEND_STRS("__auth"))) {
			zval *provider, *token, func;
			MAKE_STD_ZVAL(provider);
			MAKE_STD_ZVAL(token);
			if (header->provider) {
				ZVAL_STRING(provider, header->provider, 1);
			} else {
				ZVAL_NULL(provider);
			}

			if (header->token) {
				ZVAL_STRING(token, header->token, 1);
			} else {
				ZVAL_NULL(token);
			}

			func_params = emalloc(sizeof(zval **) * 2);
			func_params[0] = &provider;
			func_params[1] = &token;

			ZVAL_STRINGL(&func, "__auth", sizeof("__auth") - 1, 0);
			if (call_user_function_ex(NULL, &obj, &func, &retval_ptr, 2, func_params, 0, NULL TSRMLS_CC) != SUCCESS) {
				efree(func_params);
				zval_ptr_dtor(&token);
				zval_ptr_dtor(&provider);
				php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "call to api %s::%s() failed", ce->name, Z_STRVAL(func));
				goto response;
			}

			efree(func_params);
            func_params = NULL;
			zval_ptr_dtor(&token);
			zval_ptr_dtor(&provider);

			if (retval_ptr) {
               if (Z_TYPE_P(retval_ptr) == IS_BOOL && !Z_BVAL_P(retval_ptr)) {
				   zval_ptr_dtor(&retval_ptr);
				   php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "%s::__auth() return false", ce->name);
				   goto response;
			   }
			   zval_ptr_dtor(&retval_ptr);
			   retval_ptr = NULL;
			}
		}

		func_params_ht = Z_ARRVAL_P(request->parameters);
		count = zend_hash_num_elements(func_params_ht);

		if (count) {
			uint i = 0;
			func_params = emalloc(sizeof(zval **) * count);

			for (zend_hash_internal_pointer_reset(func_params_ht);
					zend_hash_get_current_data(func_params_ht, (void **) &func_params[i++]) == SUCCESS;
					zend_hash_move_forward(func_params_ht)
				);
		} else {
			func_params = NULL;
		}

		if (call_user_function_ex(NULL, &obj, request->method, &retval_ptr, count, func_params, 0, NULL TSRMLS_CC) != SUCCESS) {
			if (func_params) {
				efree(func_params);
			}
		    php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "call to api %s::%s() failed", ce->name, Z_STRVAL_P(request->method));
			goto response;
		}

		if (func_params) {
			efree(func_params);
		}

		if (retval_ptr) {
			php_yar_response_set_retval(response, retval_ptr TSRMLS_CC);
		}
	} zend_catch {
		bailout = 1;
	} zend_end_try();

	if (EG(exception)) {
		php_yar_response_set_exception(response, EG(exception) TSRMLS_CC);
		EG(exception) = NULL;
	}

response:
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4))
	php_ob_get_buffer(&output TSRMLS_CC);
	php_end_ob_buffer(0, 0 TSRMLS_CC);
#else
	if (php_output_get_contents(&output TSRMLS_CC) == FAILURE) {
		php_output_end(TSRMLS_C);
		php_yar_error(response, YAR_ERR_OUTPUT TSRMLS_CC, "unable to get ob content");
		goto response_no_output;
	}

	php_output_discard(TSRMLS_C);
#endif
	php_yar_response_alter_body(response, Z_STRVAL(output), Z_STRLEN(output), YAR_RESPONSE_REPLACE TSRMLS_CC);

response_no_output:
	php_yar_server_response(request, response TSRMLS_CC);
	php_yar_request_dtor(request TSRMLS_CC);
	php_yar_response_dtor(response TSRMLS_CC);

	if (bailout) {
		zend_bailout();
	}

	return;
} /* }}} */

static void php_yar_server_info(zval *obj TSRMLS_DC) /* {{{ */ {
	char buf[1024];
	zend_class_entry *ce = Z_OBJCE_P(obj);

	snprintf(buf, sizeof(buf), HTML_MARKUP_HEADER, ce->name);
	PHPWRITE(buf, strlen(buf));

	PHPWRITE(HTML_MARKUP_CSS, sizeof(HTML_MARKUP_CSS) - 1);
	PHPWRITE(HTML_MARKUP_SCRIPT, sizeof(HTML_MARKUP_SCRIPT) - 1);

	snprintf(buf, sizeof(buf), HTML_MARKUP_TITLE, ce->name);
	PHPWRITE(buf, strlen(buf));

    zend_hash_apply_with_argument(&ce->function_table, (apply_func_arg_t)php_yar_print_info, (void *)(ce) TSRMLS_CC);

    PHPWRITE(HTML_MARKUP_FOOTER, sizeof(HTML_MARKUP_FOOTER) - 1);
}
/* }}} */

/* {{{ proto Yar_Server::__construct($obj, $protocol = NULL)
   initizing an Yar_Server object */
PHP_METHOD(yar_server, __construct) {
    zval *obj;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "o", &obj) == FAILURE) {
        return;
    }

    zend_update_property(yar_server_ce, getThis(), "_executor", sizeof("_executor")-1, obj TSRMLS_CC);
}
/* }}} */

/* {{{ proto Yar_Server::handle()
   start service */
PHP_METHOD(yar_server, handle)
{
	char *buf;
	int buf_len;

    if (SG(headers_sent)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "headers already has been sent");
        RETURN_FALSE;
    } else {
		const char *method;
        zval *executor = NULL;

		executor = zend_read_property(yar_server_ce, getThis(), ZEND_STRL("_executor"), 0 TSRMLS_CC);
		if (!executor || IS_OBJECT != Z_TYPE_P(executor)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "executor is not a valid object");
			RETURN_FALSE;
		}

		method = SG(request_info).request_method;
		if (!method || strncasecmp(method, "POST", 4)) {
			if (YAR_G(expose_info)) {
				php_yar_server_info(executor TSRMLS_CC);
                RETURN_TRUE;
			} else {
				zend_throw_exception(yar_server_exception_ce, "server info is not allowed to access", YAR_ERR_FORBIDDEN TSRMLS_CC);
				return;
			}
		}

		php_yar_server_handle(executor TSRMLS_CC);
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ yar_server_methods */
zend_function_entry yar_server_methods[] = {
	PHP_ME(yar_server, __construct, arginfo_service___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)
	PHP_ME(yar_server, handle, arginfo_service_void, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

YAR_STARTUP_FUNCTION(service) /* {{{ */ {
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "Yar_Server", yar_server_methods);
    yar_server_ce = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_null(yar_server_ce, ZEND_STRL("_executor"), ZEND_ACC_PROTECTED TSRMLS_CC);

    return SUCCESS;
}
/* }}} */

YAR_SHUTDOWN_FUNCTION(service) /* {{{ */ {
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
