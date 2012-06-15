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
#include "Zend/zend_exceptions.h"
#include "php_hermes.h"
#include "hermes_response.h"
#include "hermes_exception.h"

zend_class_entry *hermes_exception_server_ce;
zend_class_entry *hermes_exception_client_ce;

zend_class_entry * php_hermes_get_exception_base(int root TSRMLS_DC) /* {{{ */ {
#if can_handle_soft_dependency_on_SPL && defined(HAVE_SPL) && ((PHP_MAJOR_VERSION > 5) || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 1))
	if (!root) {
		if (!spl_ce_RuntimeException) {
			zend_class_entry **pce;

			if (zend_hash_find(CG(class_table), "runtimeexception", sizeof("RuntimeException"), (void **) &pce) == SUCCESS) {
				spl_ce_RuntimeException = *pce;
				return *pce;
			}
		} else {
			return spl_ce_RuntimeException;
		}
	}
#endif

#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
	return zend_exception_get_default();
#else
	return zend_exception_get_default(TSRMLS_C);
#endif
}
/* }}} */

void php_hermes_error_ex(hermes_response_t *response, int type TSRMLS_DC, const char *format, va_list args) /* {{{ */ {
	char *msg;
	uint len;

	len = zend_vspprintf(&msg, 0, format, args);
	php_hermes_response_set_error(response, type, msg, len TSRMLS_CC);
	/* intentionally missed
	efree(msg);
	*/

	return;
} /* }}} */

void php_hermes_error(hermes_response_t *response, int type TSRMLS_DC, const char *format, ...) /* {{{ */ {
	va_list ap;

	va_start(ap, format);
	php_hermes_error_ex(response, type TSRMLS_CC, format, ap);
	va_end(ap);

	return;
}
/* }}} */

/* {{{ proto Hermes_Exception_Server::getType()
 */
PHP_METHOD(hermes_exception_server, getType)
{
	zval *type;
	type = zend_read_property(hermes_exception_server_ce, getThis(), ZEND_STRL("_type"), 0);

	RETURN_ZVAL(type, 1, 0);
}
/* }}} */

/* {{{ proto Hermes_Exception_Client::getType()
 */
PHP_METHOD(hermes_exception_client, getType)
{
	RETURN_STRINGL("Hermes_Exception_Client", sizeof("Hermes_Exception_Client") - 1, 1);
}
/* }}} */

/* {{{ hermes_exception_server_methods */
zend_function_entry hermes_exception_server_methods[] = {
	PHP_ME(hermes_exception_server, getType, NULL, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

/* {{{ hermes_exception_client_methods */
zend_function_entry hermes_exception_client_methods[] = {
	PHP_ME(hermes_exception_client, getType, NULL, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

HERMES_STARTUP_FUNCTION(exception) /* {{{ */ {
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "Hermes_Exception_Server", hermes_exception_server_methods);
	hermes_exception_server_ce = zend_register_internal_class_ex(&ce, php_hermes_get_exception_base(0 TSRMLS_CC), NULL TSRMLS_CC);
    INIT_CLASS_ENTRY(ce, "Hermes_Exception_Client", hermes_exception_client_methods);
	hermes_exception_client_ce = zend_register_internal_class_ex(&ce, php_hermes_get_exception_base(0 TSRMLS_CC), NULL TSRMLS_CC);
	zend_declare_property_stringl(hermes_exception_server_ce, ZEND_STRL("_type"), ZEND_STRL("Hermes_Exception_Server"), ZEND_ACC_PROTECTED TSRMLS_CC);

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
