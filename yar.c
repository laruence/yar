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
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_yar.h"
#include "yar_server.h"
#include "yar_client.h"
#include "yar_packager.h"
#include "yar_exception.h"
#include "yar_transport.h"

#ifdef PHP_WIN32
#include "win32/time.h"
#endif

ZEND_DECLARE_MODULE_GLOBALS(yar);

#if PHP_VERSION_ID < 70200
zend_string *php_yar_char_str[26];
#endif

/* {{{ yar_functions[]
 */
zend_function_entry yar_functions[] = {
	PHP_FE_END
};
/* }}} */

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
#ifdef ENABLE_MSGPACK
    STD_PHP_INI_ENTRY("yar.packager", "msgpack",  PHP_INI_PERDIR, OnUpdateString, default_packager, zend_yar_globals, yar_globals)
#else
    STD_PHP_INI_ENTRY("yar.packager", "php",  PHP_INI_PERDIR, OnUpdateString, default_packager, zend_yar_globals, yar_globals)
#endif
    STD_PHP_INI_ENTRY("yar.debug",  "Off", PHP_INI_ALL, OnUpdateBool, debug, zend_yar_globals, yar_globals)
    STD_PHP_INI_ENTRY("yar.expose_info",  "On", PHP_INI_PERDIR, OnUpdateBool, expose_info, zend_yar_globals, yar_globals)
    STD_PHP_INI_ENTRY("yar.connect_timeout",  "1000", PHP_INI_ALL, OnUpdateLong, connect_timeout, zend_yar_globals, yar_globals)
    STD_PHP_INI_ENTRY("yar.timeout",  "5000", PHP_INI_ALL, OnUpdateLong, timeout, zend_yar_globals, yar_globals)
	STD_PHP_INI_ENTRY("yar.content_type", "application/octet-stream", PHP_INI_ALL, OnUpdateString, content_type, zend_yar_globals, yar_globals) 
PHP_INI_END()
/* }}} */

/* {{{ void php_yar_debug(int server_side, const char *format, ...) 
 */
void php_yar_debug(int server_side, const char *format, ...) {
	va_list args;
	char buf[1024];
	char *message;
	struct timeval tv;
	struct tm m = {0};

	gettimeofday(&tv, NULL);
#ifdef PHP_WIN32
#ifdef _USE_32BIT_TIME_T
	_localtime32_s(&m, &tv.tv_sec);
#else
	localtime_s(&m, &tv.tv_sec);
#endif
#else
	php_localtime_r(&tv.tv_sec, &m);
#endif
	va_start(args, format);
	if (server_side) {
		snprintf(buf, sizeof(buf), "[Debug Yar_Server %d:%d:%d.%ld]: %s", m.tm_hour, m.tm_min, m.tm_sec, tv.tv_usec, format);
	} else {
		snprintf(buf, sizeof(buf), "[Debug Yar_Client %d:%d:%d.%ld]: %s", m.tm_hour, m.tm_min, m.tm_sec, tv.tv_usec, format);
	}
	vspprintf(&message, 0, buf, args);
	php_error(E_WARNING, "%s", message);
	efree(message);
}
/* }}} */

/* {{{ PHP_GINIT_FUNCTION
*/
PHP_GINIT_FUNCTION(yar)
{
	yar_globals->cctx.start = 0;
	yar_globals->cctx.clist = NULL;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(yar)
{
	REGISTER_INI_ENTRIES();
	REGISTER_STRINGL_CONSTANT("YAR_VERSION", PHP_YAR_VERSION, sizeof(PHP_YAR_VERSION)-1, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_HAS_MSGPACK", ENABLE_MSGPACK, CONST_CS|CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("YAR_OPT_PACKAGER", YAR_OPT_PACKAGER, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_OPT_PERSISTENT", YAR_OPT_PERSISTENT, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_OPT_TIMEOUT", YAR_OPT_TIMEOUT, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_OPT_CONNECT_TIMEOUT", YAR_OPT_CONNECT_TIMEOUT, CONST_CS|CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("YAR_OPT_HEADER", YAR_OPT_HEADER, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_OPT_RESOLVE", YAR_OPT_RESOLVE, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_OPT_PROXY", YAR_OPT_PROXY, CONST_CS|CONST_PERSISTENT);
	/* Since 2.3.0 */
	REGISTER_LONG_CONSTANT("YAR_OPT_PROVIDER", YAR_OPT_PROVIDER, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_OPT_TOKEN", YAR_OPT_TOKEN, CONST_CS|CONST_PERSISTENT);


#if PHP_VERSION_ID < 70200
	{
		unsigned char c = 'a';
		for (; c <= 'z'; c++) {
			zend_string *chr = zend_string_alloc(1, 1);
			ZSTR_VAL(chr)[0] = c;
			zend_string_hash_val(chr);
			GC_FLAGS(chr) |= IS_STR_INTERNED | IS_STR_PERMANENT;
			php_yar_char_str[c - 'a'] = chr;
		}
	}
#endif

	YAR_STARTUP(service);
	YAR_STARTUP(client);
	YAR_STARTUP(packager);
	YAR_STARTUP(transport);
	YAR_STARTUP(exception);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(yar)
{
	UNREGISTER_INI_ENTRIES();

#if PHP_VERSION_ID < 70200
	{
		unsigned char c = 0;
		for (; c < sizeof(php_yar_char_str)/sizeof(zend_string*); c++) {
			pefree(php_yar_char_str[c], 1);
		}
	}
#endif
	YAR_SHUTDOWN(service);
	YAR_SHUTDOWN(packager);

	YAR_SHUTDOWN(transport);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(yar)
{
	YAR_ACTIVATE(packager);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(yar)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(yar)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "yar support", "enabled");
	php_info_print_table_row(2, "Version", PHP_YAR_VERSION);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/** {{{ module depends
 */
#if ZEND_MODULE_API_NO >= 20050922
zend_module_dep yar_deps[] = {
	ZEND_MOD_REQUIRED("json")
#ifdef ENABLE_MSGPACK
	ZEND_MOD_REQUIRED("msgpack")
#endif
	{NULL, NULL, NULL}
};
#endif
/* }}} */

#ifdef COMPILE_DL_YAR
ZEND_GET_MODULE(yar)
#endif

/* {{{ yar_module_entry
 */
zend_module_entry yar_module_entry = {
#if ZEND_MODULE_API_NO >= 20050922
	STANDARD_MODULE_HEADER_EX, NULL,
	yar_deps,
#else
	STANDARD_MODULE_HEADER,
#endif
	"yar",
	yar_functions,
	PHP_MINIT(yar),
	PHP_MSHUTDOWN(yar),
	PHP_RINIT(yar),
	PHP_RSHUTDOWN(yar),
	PHP_MINFO(yar),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_YAR_VERSION,
#endif
	PHP_MODULE_GLOBALS(yar),
	PHP_GINIT(yar),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
