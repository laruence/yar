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
#include "php_yar.h"
#include "yar_transport.h"

/* True globals, no need for thread safety */
int le_calldata;
int le_plink;


struct _yar_transports_list {
	unsigned int size;
	unsigned int num;
	yar_transport_t **transports;
} yar_transports_list;

extern yar_transport_t yar_transport_curl;
extern yar_transport_t yar_transport_socket;

static void php_yar_plink_dtor(zend_resource *rsrc) /* {{{ */ {
	yar_persistent_le_t *le = (yar_persistent_le_t *)rsrc->ptr;
	le->dtor(le->ptr);
	free(le);
}
/* }}} */

static void php_yar_calldata_dtor(zend_resource *rsrc) /* {{{ */ {
	yar_call_data_t *entry = (yar_call_data_t *)rsrc->ptr;
	
	if (entry->uri) {
		zend_string_release(entry->uri);
	}

	if (entry->method) {
		zend_string_release(entry->method);
	}

	zval_ptr_dtor(&entry->parameters);
	zval_ptr_dtor(&entry->options);

	efree(entry);
}
/* }}} */

PHP_YAR_API yar_transport_t * php_yar_transport_get(char *name, int nlen) /* {{{ */ {
    int i = 0;
	for (;i<yar_transports_list.num;i++) {
		if (strncmp(yar_transports_list.transports[i]->name, name, nlen) == 0) {
			return yar_transports_list.transports[i];
		}
	}

	return NULL;
} /* }}} */

PHP_YAR_API int php_yar_transport_register(yar_transport_t *transport) /* {{{ */ {

	if (!yar_transports_list.size) {
	   yar_transports_list.size = 5;
	   yar_transports_list.transports = (yar_transport_t **)malloc(sizeof(yar_transport_t *) * yar_transports_list.size);
	} else if (yar_transports_list.num == yar_transports_list.size) {
	   yar_transports_list.size += 5;
	   yar_transports_list.transports = (yar_transport_t **)realloc(yar_transports_list.transports, sizeof(yar_transport_t *) * yar_transports_list.size);
	}
	yar_transports_list.transports[yar_transports_list.num] = transport;

	return yar_transports_list.num++;
} /* }}} */

YAR_STARTUP_FUNCTION(transport) /* {{{ */ {
	php_yar_transport_register(&yar_transport_curl);
	php_yar_transport_register(&yar_transport_socket);
	le_calldata = zend_register_list_destructors_ex(php_yar_calldata_dtor, NULL, "Yar Call Data", module_number);
	le_plink = zend_register_list_destructors_ex(NULL, php_yar_plink_dtor, "Yar Persistent Link", module_number);

	return SUCCESS;
} /* }}} */

YAR_ACTIVATE_FUNCTION(transport) /* {{{ */ {

	YAR_G(transport) = &yar_transport_curl;

	return SUCCESS;
} /* }}} */

YAR_SHUTDOWN_FUNCTION(transport) /* {{{ */ {
	if (yar_transports_list.size) {
		free(yar_transports_list.transports);
	}
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
