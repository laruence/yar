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

#ifndef PHP_HERMES_H
#define PHP_HERMES_H

extern zend_module_entry hermes_module_entry;
#define phpext_hermes_ptr &hermes_module_entry

#ifdef PHP_WIN32
#	define PHP_HERMES_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_HERMES_API __attribute__ ((visibility("default")))
#else
#	define PHP_HERMES_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#define HERMES_VERSION  "1.0.0"

PHP_MINIT_FUNCTION(hermes);
PHP_MSHUTDOWN_FUNCTION(hermes);
PHP_RINIT_FUNCTION(hermes);
PHP_RSHUTDOWN_FUNCTION(hermes);
PHP_MINFO_FUNCTION(hermes);

ZEND_BEGIN_MODULE_GLOBALS(hermes)
	char *default_packager;
	char *default_transport;
    struct _hermes_packager *packager;
    struct _hermes_transport *transport;
    struct _hermes_request *request;
    struct _hermes_response *response;
	zend_bool debug;
	long timeout;
	long connect_timeout;
ZEND_END_MODULE_GLOBALS(hermes)

#ifdef ZTS
#define HERMES_G(v) TSRMG(hermes_globals_id, zend_hermes_globals *, v)
#else
#define HERMES_G(v) (hermes_globals.v)
#endif

extern ZEND_DECLARE_MODULE_GLOBALS(hermes);

#define HERMES_STARTUP_FUNCTION(module)   	ZEND_MINIT_FUNCTION(hermes_##module)
#define HERMES_STARTUP(module)	 		  	ZEND_MODULE_STARTUP_N(hermes_##module)(INIT_FUNC_ARGS_PASSTHRU)
#define HERMES_SHUTDOWN_FUNCTION(module)   	ZEND_MSHUTDOWN_FUNCTION(hermes_##module)
#define HERMES_SHUTDOWN(module)	 		  	ZEND_MODULE_SHUTDOWN_N(hermes_##module)(SHUTDOWN_FUNC_ARGS_PASSTHRU)
#define HERMES_ACTIVATE_FUNCTION(module)   	ZEND_MODULE_ACTIVATE_D(hermes_##module)
#define HERMES_ACTIVATE(module)	 		  	ZEND_MODULE_ACTIVATE_N(hermes_##module)(INIT_FUNC_ARGS_PASSTHRU)
#define HERMES_DEACTIVATE_FUNCTION(module)  ZEND_MODULE_DEACTIVATE_D(hermes_##module)
#define HERMES_DEACTIVATE(module)           ZEND_MODULE_DEACTIVATE_N(hermes_##module)(SHUTDOWN_FUNC_ARGS_PASSTHRU)

#ifndef PHP_FE_END
#define PHP_FE_END { NULL, NULL, NULL }
#endif

#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 3))
#define Z_ADDREF_P 	 ZVAL_ADDREF
#define Z_REFCOUNT_P ZVAL_REFCOUNT
#define Z_DELREF_P 	 ZVAL_DELREF
#endif

#define HERMES_STASH_VARIABLES()  \
		zend_bool _old_in_compilation, _old_in_execution, _old_display_errors; \
		zend_execute_data *_old_current_execute_data; \
		int _old_http_response_code;\
		char *_old_http_status_line;\


#define HERMES_STASH_STATE()  \
		_old_in_compilation = CG(in_compilation); \
		_old_in_execution = EG(in_execution); \
		_old_current_execute_data = EG(current_execute_data); \
		_old_http_response_code = SG(sapi_headers).http_response_code; \
		_old_http_status_line = SG(sapi_headers).http_status_line; \
		_old_display_errors = PG(display_errors); \
		SG(sapi_headers).http_status_line = NULL;

#define HERMES_RESTORE_STATE() \
		CG(in_compilation) = _old_in_compilation; \
		EG(in_execution) = _old_in_execution; \
		EG(current_execute_data) = _old_current_execute_data; \
		if (SG(sapi_headers).http_status_line) { \
			efree(SG(sapi_headers).http_status_line); \
		} \
		SG(sapi_headers).http_status_line = _old_http_status_line; \
		SG(sapi_headers).http_response_code = _old_http_response_code;

#endif	/* PHP_HERMES_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
