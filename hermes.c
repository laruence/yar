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
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_hermes.h"
#include "hermes_server.h"
#include "hermes_client.h"
#include "hermes_packager.h"
#include "hermes_transport.h"

ZEND_DECLARE_MODULE_GLOBALS(hermes)

static int le_hermes;

/* {{{ hermes_functions[]
 */
zend_function_entry hermes_functions[] = {
	PHP_FE_END
};
/* }}} */

/** {{{ module depends
 */
#if ZEND_MODULE_API_NO >= 20050922
zend_module_dep hermes_deps[] = {
	ZEND_MOD_REQUIRED("json")
#ifdef ENABLE_MSGPACK
	ZEND_MOD_REQUIRED("msgpack")
#endif
	{NULL, NULL, NULL}
};
#endif
/* }}} */

/* {{{ hermes_module_entry
 */
zend_module_entry hermes_module_entry = {
#if ZEND_MODULE_API_NO >= 20050922
	STANDARD_MODULE_HEADER_EX, NULL,
	hermes_deps,
#else
	STANDARD_MODULE_HEADER,
#endif
	"hermes",
	hermes_functions,
	PHP_MINIT(hermes),
	PHP_MSHUTDOWN(hermes),
	PHP_RINIT(hermes),
	PHP_RSHUTDOWN(hermes),
	PHP_MINFO(hermes),
#if ZEND_MODULE_API_NO >= 20010901
	HERMES_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_HERMES
ZEND_GET_MODULE(hermes)
#endif

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("hermes.packager", "php",  PHP_INI_PERDIR, OnUpdateString, default_packager, zend_hermes_globals, hermes_globals)
    STD_PHP_INI_ENTRY("hermes.transport", "curl", PHP_INI_PERDIR, OnUpdateString, default_transport, zend_hermes_globals, hermes_globals)
    STD_PHP_INI_ENTRY("hermes.debug",  "Off", PHP_INI_ALL, OnUpdateBool, debug, zend_hermes_globals, hermes_globals)
    STD_PHP_INI_ENTRY("hermes.connect_timeout",  "3", PHP_INI_ALL, OnUpdateLong, timeout, zend_hermes_globals, hermes_globals)
    STD_PHP_INI_ENTRY("hermes.timeout",  "5", PHP_INI_ALL, OnUpdateLong, connect_timeout, zend_hermes_globals, hermes_globals)
PHP_INI_END()
/* }}} */

/* {{{ void php_hermes_debug(int server_side TSRMLS_DC, const char *format, ...) 
 */
void php_hermes_debug(int server_side TSRMLS_DC, const char *format, ...) {
	va_list args;
	if (!HERMES_G(debug)) {
		return;
	}
	va_start(args, format);
	if (server_side) {
		php_verror(NULL, NULL, E_NOTICE, "[Debug Hermes_Server]: %s", args TSRMLS_CC);
	} else {
		php_verror(NULL, NULL, E_NOTICE, "[Debug Hermes_Client]: %s", args TSRMLS_CC);
	}
	va_end(args);
}
/* }}} */

/* {{{ php_hermes_init_globals
 */
static void php_hermes_init_globals(zend_hermes_globals *hermes_globals) {
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(hermes)
{
	REGISTER_INI_ENTRIES();
	REGISTER_STRINGL_CONSTANT("HERMES_VERSION", HERMES_VERSION, sizeof(HERMES_VERSION)-1, CONST_CS|CONST_PERSISTENT);

	HERMES_STARTUP(service);
	HERMES_STARTUP(client);
	HERMES_STARTUP(packager);
	HERMES_STARTUP(transport);
	HERMES_STARTUP(exception);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(hermes)
{
	UNREGISTER_INI_ENTRIES();

	HERMES_SHUTDOWN(service);
	HERMES_SHUTDOWN(packager);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(hermes)
{
	HERMES_ACTIVATE(packager);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(hermes)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(hermes)
{
	DISPLAY_INI_ENTRIES();
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
