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
  |          Zhenyu  Zhang <engineer.zzy@gmail.com>                      |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_exceptions.h"
#include "php_yar.h"
#include "yar_response.h"
#include "yar_exception.h"

zend_class_entry *yar_server_exception_ce;
zend_class_entry *yar_server_request_exception_ce;
zend_class_entry *yar_server_protocol_exception_ce;
zend_class_entry *yar_server_packager_exception_ce;
zend_class_entry *yar_server_output_exception_ce;

zend_class_entry *yar_client_exception_ce;
zend_class_entry *yar_client_transport_exception_ce;
zend_class_entry *yar_client_packager_exception_ce;
zend_class_entry *yar_client_protocol_exception_ce;

zend_class_entry * php_yar_get_exception_base(int root TSRMLS_DC) /* {{{ */ {
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

void php_yar_error_ex(yar_response_t *response, int type TSRMLS_DC, const char *format, va_list args) /* {{{ */ {
	char *msg;
	uint len;

	len = vspprintf(&msg, 0, format, args);
	php_yar_response_set_error(response, type, msg, len TSRMLS_CC);
	/* intentionally missed
	efree(msg);
	*/

	return;
} /* }}} */

void php_yar_error(yar_response_t *response, int type TSRMLS_DC, const char *format, ...) /* {{{ */ {
	va_list ap;

	va_start(ap, format);
	php_yar_error_ex(response, type TSRMLS_CC, format, ap);
	va_end(ap);

	return;
}
/* }}} */

/* {{{ proto Yar_Exception_Server::getType()
 */
PHP_METHOD(yar_exception_server, getType)
{
	zval *type;
	type = zend_read_property(yar_server_exception_ce, getThis(), ZEND_STRL("_type"), 0 TSRMLS_CC);

	RETURN_ZVAL(type, 1, 0);
}
/* }}} */

/* {{{ proto Yar_Exception_Client::getType()
 */
PHP_METHOD(yar_exception_client, getType)
{
	RETURN_STRINGL("Yar_Exception_Client", sizeof("Yar_Exception_Client") - 1, 1);
}
/* }}} */

/* {{{ yar_exception_server_methods */
zend_function_entry yar_exception_server_methods[] = {
	PHP_ME(yar_exception_server, getType, NULL, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

/* {{{ yar_exception_client_methods */
zend_function_entry yar_exception_client_methods[] = {
	PHP_ME(yar_exception_client, getType, NULL, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

YAR_STARTUP_FUNCTION(exception) /* {{{ */ {
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "Yar_Server_Exception", yar_exception_server_methods);
	yar_server_exception_ce = zend_register_internal_class_ex(&ce, php_yar_get_exception_base(0 TSRMLS_CC), NULL TSRMLS_CC);

    INIT_CLASS_ENTRY(ce, "Yar_Server_Request_Exception", NULL);
	yar_server_request_exception_ce = zend_register_internal_class_ex(&ce, yar_server_exception_ce, NULL TSRMLS_CC);

    INIT_CLASS_ENTRY(ce, "Yar_Server_Protocol_Exception", NULL);
	yar_server_protocol_exception_ce = zend_register_internal_class_ex(&ce, yar_server_exception_ce, NULL TSRMLS_CC);

    INIT_CLASS_ENTRY(ce, "Yar_Server_Packager_Exception", NULL);
	yar_server_packager_exception_ce = zend_register_internal_class_ex(&ce, yar_server_exception_ce, NULL TSRMLS_CC);

    INIT_CLASS_ENTRY(ce, "Yar_Server_Output_Exception", NULL);
	yar_server_output_exception_ce = zend_register_internal_class_ex(&ce, yar_server_exception_ce, NULL TSRMLS_CC);

    INIT_CLASS_ENTRY(ce, "Yar_Client_Exception", yar_exception_client_methods);
	yar_client_exception_ce = zend_register_internal_class_ex(&ce, php_yar_get_exception_base(0 TSRMLS_CC), NULL TSRMLS_CC);
	zend_declare_property_stringl(yar_server_exception_ce, ZEND_STRL("_type"), ZEND_STRL("Yar_Exception_Server"), ZEND_ACC_PROTECTED TSRMLS_CC);

    INIT_CLASS_ENTRY(ce, "Yar_Client_Transport_Exception", NULL);
	yar_client_transport_exception_ce = zend_register_internal_class_ex(&ce, yar_client_exception_ce, NULL TSRMLS_CC);

    INIT_CLASS_ENTRY(ce, "Yar_Client_Packager_Exception", NULL);
	yar_client_packager_exception_ce = zend_register_internal_class_ex(&ce, yar_client_exception_ce, NULL TSRMLS_CC);

    INIT_CLASS_ENTRY(ce, "Yar_Client_Protocol_Exception", NULL);
	yar_client_protocol_exception_ce = zend_register_internal_class_ex(&ce, yar_client_exception_ce, NULL TSRMLS_CC);

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
