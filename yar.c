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

ZEND_DECLARE_MODULE_GLOBALS(yar);

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
    STD_PHP_INI_ENTRY("yar.transport", "curl", PHP_INI_PERDIR, OnUpdateString, default_transport, zend_yar_globals, yar_globals)
    STD_PHP_INI_ENTRY("yar.debug",  "Off", PHP_INI_ALL, OnUpdateBool, debug, zend_yar_globals, yar_globals)
    STD_PHP_INI_ENTRY("yar.expose_info",  "On", PHP_INI_PERDIR, OnUpdateBool, expose_info, zend_yar_globals, yar_globals)
    STD_PHP_INI_ENTRY("yar.connect_timeout",  "1", PHP_INI_ALL, OnUpdateLong, connect_timeout, zend_yar_globals, yar_globals)
    STD_PHP_INI_ENTRY("yar.timeout",  "5", PHP_INI_ALL, OnUpdateLong, timeout, zend_yar_globals, yar_globals)
    STD_PHP_INI_ENTRY("yar.allow_persistent",  "0", PHP_INI_ALL, OnUpdateBool, allow_persistent, zend_yar_globals, yar_globals)
PHP_INI_END()
/* }}} */

/* {{{ void php_yar_debug(int server_side TSRMLS_DC, const char *format, ...) 
 */
void php_yar_debug(int server_side TSRMLS_DC, const char *format, ...) {
	va_list args;
	if (!YAR_G(debug)) {
		return;
	}
	va_start(args, format);
	if (server_side) {
		php_verror(NULL, NULL, E_NOTICE, "[Debug Yar_Server]: %s", args TSRMLS_CC);
	} else {
		php_verror(NULL, NULL, E_NOTICE, "[Debug Yar_Client]: %s", args TSRMLS_CC);
	}
	va_end(args);
}
/* }}} */

/* {{{ PHP_GINIT_FUNCTION
*/
PHP_GINIT_FUNCTION(yar)
{
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(yar)
{
	REGISTER_INI_ENTRIES();
	REGISTER_STRINGL_CONSTANT("YAR_VERSION", YAR_VERSION, sizeof(YAR_VERSION)-1, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_OPT_PACKAGER", YAR_OPT_PACKAGER, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_OPT_PERSISTENT", YAR_OPT_PERSISTENT, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_OPT_TIMEOUT", YAR_OPT_TIMEOUT, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("YAR_OPT_CONNECT_TIMEOUT", YAR_OPT_CONNECT_TIMEOUT, CONST_CS|CONST_PERSISTENT);

	
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

	YAR_SHUTDOWN(service);
	YAR_SHUTDOWN(packager);

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
	php_info_print_table_row(2, "Version", YAR_VERSION);
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
	YAR_VERSION,
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
