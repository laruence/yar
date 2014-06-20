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
  |                                                                      |
  | Fork:  Qinghui Zeng  <zengohm@gmail.com>                             |
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
#include "yar_exception.h"
#include "yar_packager.h"
#include "yar_server.h"
#include "yar_request.h"
#include "yar_response.h"
#include "yar_protocol.h"
#include "yar_listener.h"

zend_class_entry *yar_server_ce;

/* {{{ ARG_INFO */
ZEND_BEGIN_ARG_INFO_EX(arginfo_service___construct, 0, 0, 1)
	ZEND_ARG_INFO(0, obj)
	ZEND_ARG_INFO(0, listen)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_service_set_packager, 0, 0, 1)
	ZEND_ARG_INFO(0, protocol)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_service_void, 0, 0, 1)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ proto Yar_Server::__construct($obj, $listen = NULL initizing an Yar_Server object */
PHP_METHOD(yar_server, __construct) {
    zval *obj;
    char *listen;
    long len = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "o|s", &obj, &listen, &len) == FAILURE) {
        return;
    }

    zend_update_property(yar_server_ce, getThis(), "_executor", sizeof("_executor")-1, obj TSRMLS_CC);
    
    if (len && strncasecmp(listen, ZEND_STRL("tcp://")) == 0) {
        zend_update_property_long(yar_server_ce, getThis(), ZEND_STRL("_protocol"), YAR_SERVER_PROTOCOL_TCP TSRMLS_CC);
        zend_update_property_stringl(yar_server_ce, getThis(), ZEND_STRL("_uri"), listen, len TSRMLS_CC);
    }else{
        zend_update_property_long(yar_server_ce, getThis(), ZEND_STRL("_protocol"), YAR_SERVER_PROTOCOL_HTTP TSRMLS_CC);
    }
}
/* }}} */

/* {{{ proto Yar_Server::handle()
   start service */
PHP_METHOD(yar_server, handle)
{
    
    char *msg;
    zval *executor = NULL;
    zval *protocol = NULL;
    zval *uri = NULL;

    yar_listener_t *factory;
    yar_listener_interface_t *listener;

    executor = zend_read_property(yar_server_ce, getThis(), ZEND_STRL("_executor"), 0 TSRMLS_CC);
    protocol = zend_read_property(yar_server_ce, getThis(), ZEND_STRL("_protocol"), 0 TSRMLS_CC);
    uri = zend_read_property(yar_server_ce, getThis(), ZEND_STRL("_uri"), 0 TSRMLS_CC);


    if (!executor || IS_OBJECT != Z_TYPE_P(executor)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "executor is not a valid object");
            RETURN_FALSE;
    }

    if(Z_LVAL_P(protocol)==YAR_SERVER_PROTOCOL_HTTP){
        if (SG(headers_sent)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "headers already has been sent");
            RETURN_FALSE;
        }
        factory = php_yar_listener_get(ZEND_STRL("curl") TSRMLS_CC);
    }else if(Z_LVAL_P(protocol)==YAR_SERVER_PROTOCOL_TCP){
        if (SG(headers_sent)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "server already has been started");
            RETURN_FALSE;
        }
        factory = php_yar_listener_get(ZEND_STRL("socket") TSRMLS_CC);
    }

    listener = factory->init(TSRMLS_C);
    listener->handle(listener,Z_STRVAL_P(uri),Z_STRLEN_P(uri),executor, &msg TSRMLS_CC);
    listener->close(listener);
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
        zend_declare_property_long(yar_server_ce, ZEND_STRL("_protocol"), YAR_SERVER_PROTOCOL_HTTP, ZEND_ACC_PROTECTED TSRMLS_CC);
        zend_declare_property_null(yar_server_ce, ZEND_STRL("_uri"), ZEND_ACC_PROTECTED TSRMLS_CC);
    return SUCCESS;
}
/* }}} */

YAR_SHUTDOWN_FUNCTION(service) /* {{{ */ {
    YAR_SHUTDOWN(listener);
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
