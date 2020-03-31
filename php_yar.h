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

#ifndef PHP_YAR_H
#define PHP_YAR_H

extern zend_module_entry yar_module_entry;
#define phpext_yar_ptr &yar_module_entry

#ifdef PHP_WIN32
#	define PHP_YAR_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_YAR_API __attribute__ ((visibility("default")))
#else
#	define PHP_YAR_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#define PHP_YAR_VERSION  "2.1.3-dev"

PHP_MINIT_FUNCTION(yar);
PHP_MSHUTDOWN_FUNCTION(yar);
PHP_RINIT_FUNCTION(yar);
PHP_RSHUTDOWN_FUNCTION(yar);
PHP_MINFO_FUNCTION(yar);

ZEND_BEGIN_MODULE_GLOBALS(yar)
	char *default_packager;
	char *default_transport;
    const struct _yar_packager *packager;
    const struct _yar_transport *transport;
    struct _yar_request *request;
    struct _yar_response *response;
	char *content_type;
	zend_bool debug;
	zend_bool expose_info;
	zend_bool allow_persistent;
	zend_ulong timeout;
	zend_ulong connect_timeout;
ZEND_END_MODULE_GLOBALS(yar)

#ifdef ZTS
#define YAR_G(v) TSRMG(yar_globals_id, zend_yar_globals *, v)
#else
#define YAR_G(v) (yar_globals.v)
#endif

extern ZEND_DECLARE_MODULE_GLOBALS(yar);

#define YAR_STARTUP_FUNCTION(module)   	ZEND_MINIT_FUNCTION(yar_##module)
#define YAR_STARTUP(module)	 		  	ZEND_MODULE_STARTUP_N(yar_##module)(INIT_FUNC_ARGS_PASSTHRU)
#define YAR_SHUTDOWN_FUNCTION(module)   	ZEND_MSHUTDOWN_FUNCTION(yar_##module)
#define YAR_SHUTDOWN(module)	 		  	ZEND_MODULE_SHUTDOWN_N(yar_##module)(SHUTDOWN_FUNC_ARGS_PASSTHRU)
#define YAR_ACTIVATE_FUNCTION(module)   	ZEND_MODULE_ACTIVATE_D(yar_##module)
#define YAR_ACTIVATE(module)	 		  	ZEND_MODULE_ACTIVATE_N(yar_##module)(INIT_FUNC_ARGS_PASSTHRU)
#define YAR_DEACTIVATE_FUNCTION(module)  ZEND_MODULE_DEACTIVATE_D(yar_##module)
#define YAR_DEACTIVATE(module)           ZEND_MODULE_DEACTIVATE_N(yar_##module)(SHUTDOWN_FUNC_ARGS_PASSTHRU)

#if PHP_VERSION_ID < 70200
extern zend_string *php_yar_char_str[26];
#define ZSTR_CHAR(i) php_yar_char_str[i - 'a']
#endif

#define YAR_OPT_PACKAGER 			(1<<0)	
#define YAR_OPT_PERSISTENT 			(1<<1)
#define YAR_OPT_TIMEOUT  			(1<<2)
#define YAR_OPT_CONNECT_TIMEOUT 	(1<<3)
#define YAR_OPT_HEADER				(1<<4)
#define YAR_OPT_RESOLVE 			(1<<5)

#define DEBUG_S(fmt, ...) \
	do { \
		if (UNEXPECTED(YAR_G(debug))) { \
			 php_yar_debug(1, fmt, ##__VA_ARGS__); \
		} \
	} while (0)

#define DEBUG_C(fmt, ...) \
	do { \
		if (UNEXPECTED(YAR_G(debug))) { \
			 php_yar_debug(0, fmt, ##__VA_ARGS__); \
		} \
	} while (0)

void php_yar_debug(int server_side, const char *format, ...);

#endif	/* PHP_YAR_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
