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

#ifndef PHP_YAR_TRANSPORT_H
#define PHP_YAR_TRANSPORT_H

#if 0 && HAVE_EPOLL
#define ENABLE_EPOLL
#endif

typedef struct _yar_call_data {
	ulong sequence;
	zend_string *uri;
	zend_string *method;
	zval callback;
	zval ecallback;
	zval parameters;
	zval options;
} yar_call_data_t;

typedef struct _yar_persistent_le {
	void *ptr;
	void (*dtor)(void *ptr);
} yar_persistent_le_t;

typedef int yar_concurrent_client_callback(yar_call_data_t *calldata, int status, struct _yar_response *response);

typedef struct _yar_transport_interface {
	void *data;
	int  (*open)(struct _yar_transport_interface *self, zend_string *address, long options, char **msg);
	int  (*send)(struct _yar_transport_interface *self, struct _yar_request *request, char **msg);
	struct _yar_response * (*exec)(struct _yar_transport_interface *self, struct _yar_request *request);
	int  (*setopt)(struct _yar_transport_interface *self, long type, void *value, void *addition);
	int  (*calldata)(struct _yar_transport_interface *self, yar_call_data_t *calldata);
	void (*close)(struct _yar_transport_interface *self);
} yar_transport_interface_t;

typedef struct _yar_transport_multi_interface {
    void *data;
	int (*add)(struct _yar_transport_multi_interface *self, yar_transport_interface_t *cp);
    int (*exec)(struct _yar_transport_multi_interface *self, yar_concurrent_client_callback *callback);
	void (*close)(struct _yar_transport_multi_interface *self);
} yar_transport_multi_interface_t;

typedef struct _yar_transport_multi {
	struct _yar_transport_multi_interface * (*init)();
} yar_transport_multi_t;

typedef struct _yar_transport {
	const char *name;
	struct _yar_transport_interface * (*init)();
	void (*destroy)(yar_transport_interface_t *self);
	yar_transport_multi_t *multi;
} yar_transport_t;

extern int le_calldata;
extern int le_plink;

PHP_YAR_API yar_transport_t * php_yar_transport_get(char *name, int nlen);
PHP_YAR_API int php_yar_transport_register(yar_transport_t *transport);

YAR_STARTUP_FUNCTION(transport);
YAR_SHUTDOWN_FUNCTION(transport);

#endif	/* PHP_YAR_TRANSPORT_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
