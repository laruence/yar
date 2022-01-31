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
#include "ext/standard/head.h" /* for php_header */
#include "php_yar.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_interfaces.h" /* for zend_call_method */
#include "yar_exception.h"
#include "yar_packager.h"
#include "yar_server.h"
#include "yar_request.h"
#include "yar_response.h"
#include "yar_protocol.h"
#include "yar_transport.h"

#if PHP_MAJOR_VERSION > 7
#include "yar_arginfo.h"
#else
#include "yar_legacy_arginfo.h"
#endif

zend_class_entry *yar_server_ce;

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
	"   h1 { font:bold 24px Verdana, Arial; background:#99c; padding: 12px 5px; border-bottom: 4px solid #669; box-shadow: 0 1px 4px #bbb; color: #222;}\n" \
	"   h2 { font:normal 16px/20px Courier, monospace; padding: 5px 0 8px; margin: 20px 10px 0; border-bottom: 1px solid #ddd; cursor:pointer;}\n" \
	"   p, dd { color: #555; }\n" \
	"   .api-info { padding: 10px 0; margin-left: 20px; }\n" \
	"   .api-block { margin-left: 40px;}\n" \
	"   h2 u { font-size:16px;text-decoration:none;padding:10px; }\n" \
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
	"    info.innerHTML = isHidden ? '-'  : '+';\n" \
	"   }\n" \
	"  </script>\n" \
	" </head>\n" \
	" <body>\n"

#define HTML_MARKUP_TITLE \
	" <h1>Yar Server: %s</h1>"

#define HTML_MARKUP_ENTRY_Y \
	" <h2 onclick=\"_t(this)\"><u>+</u>%s</h2>\n" \
	" <div class=\"api-block\" style=\"display:none\">\n" \
    " <pre style=\"white-space:pre-line\">%s</pre>\n" \
	" </div>\n" 

#define HTML_MARKUP_ENTRY_N \
	" <h2><u>-</u>%s</h2>\n" \
	" <div class=\"api-block\" style=\"display:none\">\n" \
	" </div>\n"

#define HTML_MARKUP_FOOTER  \
	  " <!-- powered by yar-" PHP_YAR_VERSION " -->\n"\
      "</body>\n" \
     "</html>"
/* }}} */

static char * php_yar_get_function_declaration(zend_function *fptr) /* {{{ */ {
	char *offset, *buf;
	uint32_t length = 1024;

#define REALLOC_BUF_IF_EXCEED(buf, offset, length, size) \
	if (offset - buf + size >= length) { 	\
		length += size + 1; 				\
		buf = erealloc(buf, length); 		\
	}

	if (!(fptr->common.fn_flags & ZEND_ACC_PUBLIC)) {
		return NULL;
	}

	offset = buf = (char *)emalloc(length * sizeof(char));

	if (fptr->op_array.fn_flags & ZEND_ACC_RETURN_REFERENCE) {
		*(offset++) = '&';
		*(offset++) = ' ';
	}

	if (fptr->common.scope) {
		memcpy(offset, ZSTR_VAL(fptr->common.scope->name), ZSTR_LEN(fptr->common.scope->name));
		offset += ZSTR_LEN(fptr->common.scope->name);
		*(offset++) = ':';
		*(offset++) = ':';
	}

	{
		size_t name_len = ZSTR_LEN(fptr->common.function_name);
		REALLOC_BUF_IF_EXCEED(buf, offset, length, name_len);
		memcpy(offset, ZSTR_VAL(fptr->common.function_name), name_len);
		offset += name_len;
	}

	*(offset++) = '(';
	if (fptr->common.arg_info) {
		uint32_t i, required;
		zend_arg_info *arg_info = fptr->common.arg_info;

		required = fptr->common.required_num_args;
		for (i = 0; i < fptr->common.num_args;) {
#if PHP_VERSION_ID < 80000
# if PHP_VERSION_ID >= 70200
			if (ZEND_TYPE_IS_CLASS(arg_info->type)) {
				const char *class_name;
				uint32_t class_name_len;
				zend_string *class_str = ZEND_TYPE_NAME(arg_info->type);
				class_name = ZSTR_VAL(class_str);
				class_name_len = ZSTR_LEN(class_str);
# else
			if (arg_info->class_name) {
				const char *class_name;
				uint32_t class_name_len;
				if (fptr->type == ZEND_INTERNAL_FUNCTION) {
					class_name = ((zend_internal_arg_info*)arg_info)->class_name;
					class_name_len = strlen(class_name);
				} else {
					class_name = ZSTR_VAL(arg_info->class_name);
					class_name_len = ZSTR_LEN(arg_info->class_name);
				}
# endif
				if (strncasecmp(class_name, "self", sizeof("self")) && fptr->common.scope ) {
					class_name = ZSTR_VAL(fptr->common.scope->name);
					class_name_len = ZSTR_LEN(fptr->common.scope->name);
				} else if (strncasecmp(class_name, "parent", sizeof("parent")) && fptr->common.scope->parent) {
					class_name = ZSTR_VAL(fptr->common.scope->parent->name);
					class_name_len = ZSTR_LEN(fptr->common.scope->parent->name);
				}
				REALLOC_BUF_IF_EXCEED(buf, offset, length, class_name_len);
				memcpy(offset, class_name, class_name_len);
				offset += class_name_len;
				*(offset++) = ' ';

			}
# if PHP_VERSION_ID >= 70200
			else if (ZEND_TYPE_IS_CODE(arg_info->type)) {
				uint32_t type_name_len;
				char *type_name = zend_get_type_by_const(ZEND_TYPE_CODE(arg_info->type));
# else
			else if (arg_info->type_hint) {
				uint32_t type_name_len;
				char *type_name = zend_get_type_by_const(arg_info->type_hint);
# endif
				type_name_len = strlen(type_name);
				REALLOC_BUF_IF_EXCEED(buf, offset, length, type_name_len);
				memcpy(offset, type_name, type_name_len);
				offset += type_name_len;
				*(offset++) = ' ';
			}
#else
			if (ZEND_TYPE_IS_SET(arg_info->type)) {
				zend_string *type_str = zend_type_to_string(arg_info->type);
				REALLOC_BUF_IF_EXCEED(buf, offset, length, ZSTR_LEN(type_str));
				memcpy(offset, ZSTR_VAL(type_str), ZSTR_LEN(type_str));
				offset += ZSTR_LEN(type_str);
				*(offset++) = ' ';
				zend_string_release(type_str);
			}
#endif

#if PHP_VERSION_ID < 80000
			if (arg_info->pass_by_reference)
#else
			if (ZEND_ARG_SEND_MODE(arg_info))
#endif
			{
				*(offset++) = '&';
			}
			*(offset++) = '$';

			if (arg_info->name) {
				const char *name;
				uint32_t name_len;
				if (fptr->type == ZEND_INTERNAL_FUNCTION) {
					name = ((zend_internal_arg_info*)arg_info)->name;
					name_len = strlen(name);
				} else {
					name = ZSTR_VAL(arg_info->name);
					name_len = ZSTR_LEN(arg_info->name);
				}
				REALLOC_BUF_IF_EXCEED(buf, offset, length, name_len);
				memcpy(offset, name, name_len);
				offset += name_len;
			} else {
				uint32_t idx = i;
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
						uint32_t idx  = i;
						zend_op *op = ((zend_op_array *)fptr)->opcodes;
						zend_op *end = op + ((zend_op_array *)fptr)->last;

						++idx;
						while (op < end) {
							if ((op->opcode == ZEND_RECV || op->opcode == ZEND_RECV_INIT)
									&& op->op1.num == (long)idx
									) {
								precv = op;
							}
							++op;
						}
					}
					if (precv && precv->opcode == ZEND_RECV_INIT
						   	&& precv->op2_type != IS_UNUSED
							) {
#if PHP_VERSION_ID < 70300
						zval *zv = RT_CONSTANT(&fptr->op_array, precv->op2);
#else
						zval *zv = RT_CONSTANT(precv, precv->op2);
#endif

						if (Z_TYPE_P(zv) == IS_TRUE) {
                            memcpy(offset, "true", 4);
                            offset += 4;
                        } else if (Z_TYPE_P(zv) == IS_FALSE) {
							memcpy(offset, "false", 5);
							offset += 5;
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
#if PHP_VERSION_ID < 70400
							int use_copy;
							zval zv_copy;
							use_copy = zend_make_printable_zval(zv, &zv_copy);
							REALLOC_BUF_IF_EXCEED(buf, offset, length, Z_STRLEN(zv_copy));
							memcpy(offset, Z_STRVAL(zv_copy), Z_STRLEN(zv_copy));
							offset += Z_STRLEN(zv_copy);
							if (use_copy) {
								zval_dtor(&zv_copy);
							}
#else
							zend_string *tmp_zv_str;
							zend_string *zv_str = zval_get_tmp_string(zv, &tmp_zv_str);
							REALLOC_BUF_IF_EXCEED(buf, offset, length, ZSTR_LEN(zv_str));
							memcpy(offset, ZSTR_VAL(zv_str), ZSTR_LEN(zv_str));
							zend_tmp_string_release(tmp_zv_str);
#endif
						}
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
#if PHP_VERSION_ID >= 80000
	if (fptr->common.fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
		zend_type return_type = fptr->common.arg_info[-1].type;
		zend_string *type_str = zend_type_to_string(return_type);
		*(offset++) = ':';
		*(offset++) = ' ';
		REALLOC_BUF_IF_EXCEED(buf, offset, length, ZSTR_LEN(type_str));
		memcpy(offset, ZSTR_VAL(type_str), ZSTR_LEN(type_str));
		offset += ZSTR_LEN(type_str);
		*(offset++) = ' ';
		zend_string_release(type_str);
	}
#endif

	*offset = '\0';

	return buf;
}
/* }}} */

static int php_yar_print_info(zval *ptr, void *argument) /* {{{ */ {
    zend_function *f = Z_FUNC_P(ptr);
	smart_str *out = (smart_str *)argument;

    if (f->common.fn_flags & ZEND_ACC_PUBLIC 
		&& f->common.function_name && *(ZSTR_VAL(f->common.function_name)) != '_') {
        char *prototype = NULL;
		if ((prototype = php_yar_get_function_declaration(f))) {
			char *buf, *doc_comment = NULL;
			if (f->type == ZEND_USER_FUNCTION) {
				if (f->op_array.doc_comment != NULL) {
					doc_comment = (char *)ZSTR_VAL(f->op_array.doc_comment);
				}
			}
			if (doc_comment) {
				spprintf(&buf, 0, HTML_MARKUP_ENTRY_Y, prototype, doc_comment);
			} else {
				spprintf(&buf, 0, HTML_MARKUP_ENTRY_N, prototype);
			}
			efree(prototype);
			smart_str_appendl(out, buf, strlen(buf));
            efree(buf);
		}
    }

	return ZEND_HASH_APPLY_KEEP;
} /* }}} */

static void php_yar_server_response_header(size_t content_lenth, void *packager_info) /* {{{ */ {
	sapi_header_line ctr = {0};
	char header_line[64];

	ctr.line_len = snprintf(header_line, sizeof(header_line), "Content-Length: %ld", content_lenth);
	ctr.line = header_line;
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr);

	ctr.line_len = snprintf(header_line, sizeof(header_line), "Content-Type: %s", YAR_G(content_type));
	ctr.line = header_line;
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr);

	ctr.line_len = snprintf(header_line, sizeof(header_line), "Cache-Control: no-cache");
	ctr.line = header_line;
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr);

	/*
	ctr.line_len = snprintf(header_line, sizeof(header_line), "Connection: Keep-Alive");
	ctr.line = header_line;
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr);
	*/

	php_header();

	return;
} /* }}} */

static void php_yar_server_response(yar_request_t *request, yar_response_t *response, char *pkg_name) /* {{{ */ {
	zval rv;
	char *err_msg;
	zend_array ret;
	zend_string *payload;
	yar_header_t header = {0};

	zend_hash_init(&ret, 8, NULL, NULL, 0);

	ZVAL_LONG(&rv, response->id);
	zend_hash_add(&ret, ZSTR_CHAR('i'), &rv);
	ZVAL_LONG(&rv, response->status);
	zend_hash_add(&ret, ZSTR_CHAR('s'), &rv);

	if (response->out && ZSTR_LEN(response->out)) {
		ZVAL_STR(&rv, response->out);
		zend_hash_add(&ret, ZSTR_CHAR('o'), &rv);
	}
	if (!Z_ISUNDEF(response->retval)) {
		zend_hash_add(&ret, ZSTR_CHAR('r'), &response->retval);
	}
	if (!Z_ISUNDEF(response->err)) {
		zend_hash_add(&ret, ZSTR_CHAR('e'), &response->err);
	}

	ZVAL_ARR(&rv, &ret);
    if (!(payload = php_yar_packager_pack(pkg_name, &rv, &err_msg))) {
		zend_hash_destroy(&ret);
		php_yar_error(response, YAR_ERR_PACKAGER, "%s", err_msg);
		efree(err_msg);
		return;
	}
	zend_hash_destroy(&ret);

	php_yar_protocol_render(&header, request? request->id : 0, "PHP Yar Server", NULL, ZSTR_LEN(payload), 0);

	DEBUG_S("%ld: server response: packager '%s', len '%ld', content '%.32s'",
			request? request->id : 0, ZSTR_VAL(payload), ZSTR_LEN(payload) - 8, ZSTR_VAL(payload) + 8);

	php_yar_server_response_header(sizeof(yar_header_t) + ZSTR_LEN(payload), ZSTR_VAL(payload));
	PHPWRITE((char *)&header, sizeof(yar_header_t));
	if (ZSTR_LEN(payload)) {
		PHPWRITE(ZSTR_VAL(payload), ZSTR_LEN(payload));
	}

	zend_string_release(payload);

	return;

} /* }}} */

static inline int php_yar_server_auth(zval *obj, yar_header_t *header, yar_response_t *response) /* {{{ */ {
	zval ret;
	zend_function *fbc;
	zend_class_entry *ce = Z_OBJCE_P(obj);

	if ((fbc = zend_hash_str_find_ptr(&ce->function_table, "__auth", sizeof("__auth") - 1)) == NULL) {
		return 1;
	}

	if (!(fbc->common.fn_flags & ZEND_ACC_PROTECTED)) {
		return 1;
	}

	YAR_TRY {
		zval auth_params[2];

		ZVAL_STRINGL(&auth_params[0], (char*)header->provider, MIN(strlen(header->provider), 32));
		ZVAL_STRINGL(&auth_params[1], (char*)header->token, MIN(strlen(header->token), 32));

#if PHP_VERSION_ID < 80000
		zend_call_method_with_2_params(obj, ce, NULL, "__auth", &ret, auth_params, auth_params + 1);
#else
		zend_call_known_instance_method(fbc, Z_OBJ_P(obj), &ret, 2, auth_params);
#endif
		zend_string_release(Z_STR(auth_params[0]));
		zend_string_release(Z_STR(auth_params[1]));
	} YAR_CATCH();

	if (EG(exception)) {
		zend_object *exception = EG(exception);
		php_yar_response_set_exception(response, exception);
		EG(exception) = NULL; /* exception may have __destruct will be called */
		OBJ_RELEASE(exception);
		zend_clear_exception();
		return 0;
	}

	if (Z_TYPE(ret) == IS_FALSE) {
		php_yar_error(response, YAR_ERR_FORBIDDEN, "authentication failed");
		return 0;
	}

	zval_ptr_dtor(&ret);
	return 1;
}
/* }}} */

static inline int php_yar_server_call(zval *obj, yar_request_t *request, yar_response_t *response) /* {{{ */ {
	zend_class_entry *ce = Z_OBJCE_P(obj);
	int bailout = 0;

#if PHP_VERSION_ID < 80000
	zend_string *method = zend_string_tolower(request->method);
	if (!zend_hash_exists(&ce->function_table, method)) {
		zend_string_release(method);
		php_yar_error(response, YAR_ERR_REQUEST, "call to undefined api %s::%s()", ZSTR_VAL(ce->name), ZSTR_VAL(request->method));
		return 0;
	}
	zend_string_release(method);
#else
	zend_function *fbc;
	if ((fbc = zend_hash_find_ptr_lc(&ce->function_table, request->method)) == NULL) {
		php_yar_error(response, YAR_ERR_REQUEST, "call to undefined api %s::%s()", ZSTR_VAL(ce->name), ZSTR_VAL(request->method));
		return 0;
	}
#endif
	YAR_TRY {
		int count;
		zval *func_params;
		zval retval;
#if PHP_VERSION_ID < 80000
		zval func;
#endif
		HashTable *func_params_ht;

		func_params_ht = request->parameters;
		if ((count = zend_hash_num_elements(func_params_ht))) {
			int i = 0;
			zval *zv;

			func_params = safe_emalloc(count, sizeof(zval), 0);
			ZEND_HASH_FOREACH_VAL(func_params_ht, zv) {
				ZVAL_COPY(&func_params[i++], zv);
			} ZEND_HASH_FOREACH_END();
		} else {
			func_params = NULL;
		}

#if PHP_VERSION_ID < 80000
		ZVAL_STR(&func, request->method);
		if (FAILURE == call_user_function(NULL, obj, &func, &retval, count, func_params)) {
			if (count) {
				int i = 0;
				for (; i < count; i++) {
					zval_ptr_dtor(&func_params[i]);
				}
				efree(func_params);
			}
			php_yar_error(response, YAR_ERR_REQUEST, "call to api %s::%s() failed", ZSTR_VAL(ce->name), ZSTR_VAL(request->method));
			return 0;
		}
#else
		zend_call_known_instance_method(fbc, Z_OBJ_P(obj), &retval, count, func_params);
#endif

		if (!Z_ISUNDEF(retval)) {
			php_yar_response_set_retval(response, &retval);
			zval_ptr_dtor(&retval);
		}

		if (count) {
			int i = 0;
			for (; i < count; i++) {
				zval_ptr_dtor(&func_params[i]);
			}
			efree(func_params);
		}
	} YAR_CATCH (bailout = 1);

	if (UNEXPECTED(EG(exception))) {
		zend_object *exception = EG(exception);
#if PHP_VERSION_ID < 80000
		php_yar_response_set_exception(response, exception);
#else
		if (!zend_is_unwind_exit(exception)) {
			php_yar_response_set_exception(response, exception);
		} else {
			bailout = 1;
		}
#endif
		EG(exception) = NULL; /* exception may have __destruct will be called */
		OBJ_RELEASE(exception);
		zend_clear_exception();
	}

	return bailout;
}
/* }}} */

static void php_yar_server_info(zval *obj) /* {{{ */ {
	zval ret;
	smart_str out = {0};
	zend_function *fbc;
	zend_class_entry *ce = Z_OBJCE_P(obj);

	/* build html markup */
	size_t len = sizeof(HTML_MARKUP_HEADER) + ZSTR_LEN(ce->name)
	   	+ sizeof(HTML_MARKUP_CSS) + sizeof(HTML_MARKUP_SCRIPT) + sizeof(HTML_MARKUP_TITLE) + ZSTR_LEN(ce->name) - 4;

	smart_str_alloc(&out, len, 0);
	ZSTR_LEN(out.s) = sprintf(ZSTR_VAL(out.s), HTML_MARKUP_HEADER, ZSTR_VAL(ce->name));
	smart_str_appendl(&out, HTML_MARKUP_CSS, sizeof(HTML_MARKUP_CSS) - 1);
	smart_str_appendl(&out, HTML_MARKUP_SCRIPT, sizeof(HTML_MARKUP_SCRIPT) - 1);
	ZSTR_LEN(out.s) += sprintf(ZSTR_VAL(out.s), HTML_MARKUP_TITLE, ZSTR_VAL(ce->name));

	zend_hash_apply_with_argument(&ce->function_table, (apply_func_arg_t)php_yar_print_info, (void *)(&out));

	smart_str_erealloc(&out, ZSTR_LEN(out.s) + sizeof(HTML_MARKUP_FOOTER) - 1); /* include tailing zero byte */
	smart_str_appendl(&out, HTML_MARKUP_FOOTER, sizeof(HTML_MARKUP_FOOTER) - 1);
	smart_str_0(&out);

	if ((fbc = zend_hash_str_find_ptr(&ce->function_table, "__info", sizeof("__info") - 1))
		&& (fbc->common.fn_flags & ZEND_ACC_PROTECTED)) {
		YAR_TRY {
			zval html;
			ZVAL_STR_COPY(&html, out.s);
#if PHP_VERSION_ID < 80000
			zend_call_method_with_1_params(obj, ce, NULL, "__info", &ret, &html);
#else
			zend_call_known_instance_method(fbc, Z_OBJ_P(obj), &ret, 1, &html);
#endif
			zval_ptr_dtor(&html);
		} YAR_CATCH(/*html memleak? */);

		if (EG(exception)) {
			smart_str_free(&out);
			zval_ptr_dtor(&ret);
			return;
		}

		if (Z_TYPE(ret) == IS_STRING) {
			PHPWRITE(Z_STRVAL(ret), Z_STRLEN(ret));
			smart_str_free(&out);
			zval_ptr_dtor(&ret);
			return;
		} else {
			zval_ptr_dtor(&ret);
		}
	}

	PHPWRITE(ZSTR_VAL(out.s), ZSTR_LEN(out.s));
	smart_str_free(&out);
	return;
}
/* }}} */

static void php_yar_server_handle(zval *obj) /* {{{ */ {
	size_t len = 0;
	int bailout = 0;
	char buf[RECV_BUF_SIZE];
	char *payload, *err_msg;
	char *pkg_name = NULL;
	size_t payload_len;
	zval *post_data, rv;
	yar_header_t *header;
	yar_request_t *request = NULL;
	smart_str raw_data = {0};

	yar_response_t *response = php_yar_response_instance();
	if (EXPECTED(SG(request_info.request_body))) {
		php_stream *s = SG(request_info).request_body;

		if (UNEXPECTED(FAILURE == php_stream_rewind(s))) {
			php_yar_error(response, YAR_ERR_PACKAGER, "empty request");
			DEBUG_S("0: empty request");
			goto response;
		}
		while (!php_stream_eof(s)) {
			len += php_stream_read(s, buf + len, sizeof(buf) - len);
			if (len == sizeof(buf) || raw_data.s) {
				smart_str_appendl(&raw_data, buf, len);
				len = 0;
			}
		}
		if (len) {
			buf[len] = '\0'; /* json seems require tail zero-byte */
			payload = buf;
			payload_len = len;
		} else if (raw_data.s) {
			smart_str_0(&raw_data);
			payload = ZSTR_VAL(raw_data.s);
			payload_len = ZSTR_LEN(raw_data.s);
		} else {
			php_yar_error(response, YAR_ERR_PACKAGER, "empty request body");
			DEBUG_S("0: empty request '%s'");
			goto response;
		}
	} else {
		php_yar_error(response, YAR_ERR_PACKAGER, "empty request");
		DEBUG_S("0: empty request");
		goto response;
	}

	if (UNEXPECTED(!(header = php_yar_protocol_parse(payload)))) {
		php_yar_error(response, YAR_ERR_PACKAGER, "malformed request header '%.10s'", payload);
		DEBUG_S("0: malformed request '%s'", payload);
		goto response;
	}

	DEBUG_S("%ld: accpect rpc request form '%s'",
			header->id, header->provider? (char *)header->provider : "Yar PHP " PHP_YAR_VERSION);

	payload += sizeof(yar_header_t);
	payload_len -= sizeof(yar_header_t);

	if (UNEXPECTED(!(post_data = php_yar_packager_unpack(payload, payload_len, &err_msg, &rv)))) {
        php_yar_error(response, YAR_ERR_PACKAGER, err_msg);
		efree(err_msg);
		goto response;
	}

	pkg_name = payload;
	request = php_yar_request_unpack(post_data);
	zval_ptr_dtor(post_data);

	if (UNEXPECTED(!php_yar_request_valid(request, response, &err_msg))) {
		php_yar_error(response, YAR_ERR_REQUEST, "%s", err_msg);
		efree(err_msg);
		goto response;
	}

	if (!php_yar_server_auth(obj, header, response)) {
		goto response;
	}

	if (UNEXPECTED(php_output_start_user(NULL, 0, PHP_OUTPUT_HANDLER_STDFLAGS) == FAILURE)) {
		php_yar_error(response, YAR_ERR_OUTPUT, "start output buffer failed");
		goto response;
	}

	bailout = php_yar_server_call(obj, request, response);
	if (UNEXPECTED(php_output_get_contents(&rv) == FAILURE)) {
		php_output_discard();
		goto response;
	}
	php_yar_response_alter_body(response, Z_STR(rv), YAR_RESPONSE_REPLACE);
	php_output_discard();

response:
	php_yar_server_response(request, response, pkg_name);
	if (request) {
		php_yar_request_destroy(request);
	}

	smart_str_free(&raw_data);

	php_yar_response_destroy(response);

	if (bailout) {
#if PHP_VERSION_ID < 80000
		zend_bailout();
#else
		zend_throw_unwind_exit();
#endif
	}
} /* }}} */

/* {{{ proto Yar_Server::__construct($obj)
   initizing an Yar_Server object */
PHP_METHOD(yar_server, __construct) {
    zval *obj;

    if (zend_parse_parameters_throw(ZEND_NUM_ARGS(), "o", &obj) == FAILURE) {
        return;
    }

#if PHP_VERSION_ID < 80000
    zend_update_property(yar_server_ce, getThis(), "_executor", sizeof("_executor")-1, obj);
#else
    zend_update_property(yar_server_ce, Z_OBJ_P(getThis()), "_executor", sizeof("_executor")-1, obj);
#endif
}
/* }}} */

/* {{{ proto Yar_Server::setOpt(int $type, mixed $value)
   set options */
PHP_METHOD(yar_server, setOpt)
{
}
/* }}} */

/* {{{ proto Yar_Server::handle()
   start service */
PHP_METHOD(yar_server, handle)
{
	const char *method;
	zval *executor, rv;

    if (UNEXPECTED(SG(headers_sent))) {
		php_error_docref(NULL, E_WARNING, "headers already has been sent");
        RETURN_FALSE;
    }

#if PHP_VERSION_ID < 80000
	executor = zend_read_property(yar_server_ce, getThis(), ZEND_STRL("_executor"), 0, &rv);
#else
	executor = zend_read_property(yar_server_ce, Z_OBJ_P(getThis()), ZEND_STRL("_executor"), 0, &rv);
#endif
	if (IS_OBJECT != Z_TYPE_P(executor)) {
		php_error_docref(NULL, E_WARNING, "executor is not a valid object");
		RETURN_FALSE;
	}

	method = SG(request_info).request_method;
	if (!method || strncasecmp(method, "POST", 4)) {
		if (YAR_G(expose_info)) {
			php_yar_server_info(executor);
			RETURN_TRUE;
		} else {
			zend_throw_exception(yar_server_exception_ce, "server info is not allowed to access", YAR_ERR_FORBIDDEN);
			return;
		}
	}

	php_yar_server_handle(executor);

	RETURN_TRUE;
}
/* }}} */

/* {{{ yar_server_methods */
zend_function_entry yar_server_methods[] = {
	PHP_ME(yar_server, __construct, arginfo_class_Yar_Server___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)
	PHP_ME(yar_server, handle, arginfo_class_Yar_Server_handle, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

YAR_STARTUP_FUNCTION(server) /* {{{ */ {
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "Yar_Server", yar_server_methods);
    yar_server_ce = zend_register_internal_class(&ce);
	zend_declare_property_null(yar_server_ce, ZEND_STRL("_executor"), ZEND_ACC_PROTECTED);

    return SUCCESS;
}
/* }}} */

YAR_SHUTDOWN_FUNCTION(server) /* {{{ */ {
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
