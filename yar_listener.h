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
  | Yar Author:  Xinchen Hui   <laruence@php.net>                        |
  |              Zhenyu  Zhang <zhangzhenyu@php.net>                     |
  |                                                                      |
  | Fork:        Qinghui Zeng  <zengohm@gmail.com>                       |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_YAR_LISTENER_H
#define	PHP_YAR_LISTENER_H

typedef struct _yar_listener_interface {
	void *data;
        zval *executor;
	int  (*listen)(struct _yar_listener_interface *self, char *address, uint len, zval *executor, char **err_msg TSRMLS_DC);
        int  (*accept)(struct _yar_listener_interface *self, void* client TSRMLS_DC);
        int  (*recv)(struct _yar_listener_interface *self, void* client, struct _yar_request ** TSRMLS_DC);
	int  (*exec)(struct _yar_listener_interface *self, struct _yar_request *request, struct _yar_response * TSRMLS_DC);
	void (*close)(struct _yar_listener_interface *self TSRMLS_DC);
} yar_listener_interface_t;

/*typedef struct _yar_call_data {
	ulong sequence;
	char *uri;
	uint ulen;
	char *method;
	uint mlen;
	zval *callback;
	zval *ecallback;
	zval *parameters;
	zval *options;
} yar_call_data_t;

typedef int yar_concurrent_server_callback(yar_call_data_t *calldata, int status, struct _yar_request *request TSRMLS_DC);
*/
typedef struct _yar_listener_multi_interface {
    void *data;
	int (*add)(struct _yar_listener_multi_interface *self, yar_listener_interface_t *cp TSRMLS_DC);
  /*  int (*exec)(struct _yar_listener_multi_interface *self, yar_concurrent_server_callback *callback TSRMLS_DC); */
	void (*close)(struct _yar_listener_multi_interface *self TSRMLS_DC);
} yar_listener_multi_interface_t;

typedef struct _yar_listener_multi {
	struct _yar_listener_multi_interface * (*init)(TSRMLS_D);
} yar_listener_multi_t;

typedef struct _yar_listener {
	const char *name;
	yar_listener_interface_t * (*init)(TSRMLS_D);
	void (*destroy)(yar_listener_interface_t *self TSRMLS_DC);
	yar_listener_multi_t *multi;
} yar_listener_t;

PHP_YAR_API yar_listener_t * php_yar_listener_get(char *name, int nlen TSRMLS_DC);
PHP_YAR_API int php_yar_listener_register(yar_listener_t *listener TSRMLS_DC);

YAR_STARTUP_FUNCTION(listener);
YAR_SHUTDOWN_FUNCTION(listener);


#endif	/* YAR_LISTENER_H */

