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

#define YAR_VERSION  "1.1.2"

PHP_MINIT_FUNCTION(yar);
PHP_MSHUTDOWN_FUNCTION(yar);
PHP_RINIT_FUNCTION(yar);
PHP_RSHUTDOWN_FUNCTION(yar);
PHP_MINFO_FUNCTION(yar);

ZEND_BEGIN_MODULE_GLOBALS(yar)
	char *default_packager;
	char *default_transport;
    struct _yar_packager *packager;
    struct _yar_transport *transport;
    struct _yar_request *request;
    struct _yar_response *response;
	zend_bool debug;
	zend_bool expose_info;
	zend_bool allow_persistent;
	ulong timeout;
	ulong connect_timeout;
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

#ifndef PHP_FE_END
#define PHP_FE_END { NULL, NULL, NULL }
#endif

#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 3))
#define Z_ADDREF_P 	 ZVAL_ADDREF
#define Z_REFCOUNT_P ZVAL_REFCOUNT
#define Z_DELREF_P 	 ZVAL_DELREF
#endif

#define YAR_OPT_PACKAGER 			0x01
#define YAR_OPT_PERSISTENT 			0x02
#define YAR_OPT_TIMEOUT  			0x04
#define YAR_OPT_CONNECT_TIMEOUT 	0x08

#define YAR_STASH_VARIABLES()  \
		zend_bool _old_in_compilation, _old_in_execution, _old_display_errors; \
		zend_execute_data *_old_current_execute_data; \
		int _old_http_response_code;\
		char *_old_http_status_line;\


#define YAR_STASH_STATE()  \
		_old_in_compilation = CG(in_compilation); \
		_old_in_execution = EG(in_execution); \
		_old_current_execute_data = EG(current_execute_data); \
		_old_http_response_code = SG(sapi_headers).http_response_code; \
		_old_http_status_line = SG(sapi_headers).http_status_line; \
		_old_display_errors = PG(display_errors); \
		SG(sapi_headers).http_status_line = NULL;

#define YAR_RESTORE_STATE() \
		CG(in_compilation) = _old_in_compilation; \
		EG(in_execution) = _old_in_execution; \
		EG(current_execute_data) = _old_current_execute_data; \
		if (SG(sapi_headers).http_status_line) { \
			efree(SG(sapi_headers).http_status_line); \
		} \
		SG(sapi_headers).http_status_line = _old_http_status_line; \
		SG(sapi_headers).http_response_code = _old_http_response_code;

#endif	/* PHP_YAR_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
