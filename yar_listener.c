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
  | Fork:    Qinghui Zeng  <zengohm@gmail.com>                           |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_yar.h"
#include "yar_listener.h"

/* True globals, no need for thread safety */
//int le_calldata;
//int le_plink;

struct _yar_listeners_list {
	unsigned int size;
	unsigned int num;
	yar_listener_t **listeners;
} yar_listeners_list;

extern yar_listener_t yar_listener_curl;
extern yar_listener_t yar_listener_socket;

PHP_YAR_API yar_listener_t * php_yar_listener_get(char *name, int nlen TSRMLS_DC) /* {{{ */ {
    int i = 0;
	for (;i<yar_listeners_list.num;i++) {
		if (strncmp(yar_listeners_list.listeners[i]->name, name, nlen) == 0) {
			return yar_listeners_list.listeners[i];
		}
	}

	return NULL;
} /* }}} */

PHP_YAR_API int php_yar_listener_register(yar_listener_t *listener TSRMLS_DC) /* {{{ */ {

	if (!yar_listeners_list.size) {
	   yar_listeners_list.size = 5;
	   yar_listeners_list.listeners = (yar_listener_t **)malloc(sizeof(yar_listener_t *) * yar_listeners_list.size);
	} else if (yar_listeners_list.num == yar_listeners_list.size) {
	   yar_listeners_list.size += 5;
	   yar_listeners_list.listeners = (yar_listener_t **)realloc(yar_listeners_list.listeners, sizeof(yar_listener_t *) * yar_listeners_list.size);
	}
	yar_listeners_list.listeners[yar_listeners_list.num] = listener;

	return yar_listeners_list.num++;
} /* }}} */

YAR_STARTUP_FUNCTION(listener) /* {{{ */ {
	php_yar_listener_register(&yar_listener_curl TSRMLS_CC);
	php_yar_listener_register(&yar_listener_socket TSRMLS_CC);
	//le_calldata = zend_register_list_destructors_ex(php_yar_calldata_dtor, NULL, "Yar Call Data", module_number);
	//le_plink = zend_register_list_destructors_ex(NULL, php_yar_plink_dtor, "Yar Persistent Link", module_number);

	return SUCCESS;
} /* }}} */

YAR_ACTIVATE_FUNCTION(listener) /* {{{ */ {

	//YAR_G(listener) = &yar_listener_curl;

	return SUCCESS;
} /* }}} */

YAR_SHUTDOWN_FUNCTION(listener) /* {{{ */ {
    if (yar_listeners_list.size) {
        free(yar_listeners_list.listeners);
    }
    return SUCCESS;
} /* }}} */
