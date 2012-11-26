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
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_network.h"

#ifndef PHP_WIN32
#define php_select(m, r, w, e, t)   select(m, r, w, e, t)
#else
#include "win32/select.h"
#endif

#include "php_yar.h"
#include "yar_transport.h"
#include "yar_packager.h"
#include "yar_exception.h"
#include "ext/standard/php_var.h" /* for serialize */
#include "ext/standard/php_smart_str.h" /* for smart string */

#ifdef ENABLE_EPOLL
#include <sys/epoll.h>
#define YAR_EPOLL_MAX_SIZE 128
#endif

typedef struct _yar_socket_data_t {
	php_stream *stream;
} yar_socket_data_t;

int php_yar_socket_open(yar_transport_interface_t *self, char *address, uint len, char *hostname, long options TSRMLS_DC) /* {{{ */ {
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;
	struct timeval tv;
	php_stream *stream = NULL;
	char *errstr = NULL;
	int err;

	tv.tv_sec = YAR_G(connect_timeout);
	tv.tv_usec = 0;

	stream = php_stream_xport_create(address, len, REPORT_ERRORS, STREAM_XPORT_CLIENT|STREAM_XPORT_CONNECT, NULL, &tv, NULL, &errstr, &err);
	if (stream == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to connect to %s (%s)", address, errstr);
		efree(errstr);
		return 0;
	}

	data->stream = stream;
		
	return 1;
} /* }}} */

void php_yar_socket_close(yar_transport_interface_t* self TSRMLS_DC) /* {{{ */ {
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;

	if (!data) {
		return;
	}

	if (data->stream) {
		php_stream_close(data->stream);
	}

	efree(data);
	efree(self);

	return;
}
/* }}} */

int php_yar_socket_exec(yar_transport_interface_t* self, char **response, size_t *len, uint *code, char **msg TSRMLS_DC) /* {{{ */ {
	int fd, retval, recvd;
	fd_set rfds;
	char buf[1024];
	struct timeval tv;
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;
	*response = NULL;
	*len = 0;

	FD_ZERO(&rfds);
	if (SUCCESS == php_stream_cast(data->stream, PHP_STREAM_AS_FD_FOR_SELECT | PHP_STREAM_CAST_INTERNAL, (void*)&fd, 1) && fd >= 0) {
		PHP_SAFE_FD_SET(fd, &rfds);
	} else {
		spprintf(msg, 0, "Unable cast socket fd form stream");
		return 0;
	}

	tv.tv_sec = YAR_G(timeout);
	tv.tv_usec = 0;

wait_io:
	while ((retval = php_select(fd+1, &rfds, NULL, NULL, &tv)) == 0);
	if (retval == -1) {
		spprintf(msg, 0, "Unable to select %d '%s'", fd, strerror(errno));
		return 0;
	}

	if (PHP_SAFE_FD_ISSET(fd, &rfds)) {
		while ((recvd = php_stream_xport_recvfrom(data->stream, buf, sizeof(buf), 0, NULL, NULL, NULL, NULL TSRMLS_CC)) > 0) {
			if (*response) {
				*len += recvd;
				*response = erealloc(*response, *len + recvd + 1);
				memcpy(*response + *len - 1, buf, recvd);
				*len += recvd;
			} else {
				*response = emalloc(recvd + 1);
				memcpy(*response, buf, recvd);
				*len = recvd;
			}
		}
	} else {
		goto wait_io;
	}

	if (!(*response)) {
		spprintf(msg, 0, "server responsed emptry response");
		return 0;
	}

	(*response)[*len] = '\0';
	return 1;
} /* }}} */

int php_yar_socket_send(yar_transport_interface_t* self, char *payload, size_t len TSRMLS_DC) /* {{{ */ {
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;
	return php_stream_xport_sendto(data->stream, payload, len, 0, NULL, 0 TSRMLS_CC);
} /* }}} */

int php_yar_socket_setopt(yar_transport_interface_t* self, long type, void *value, void *addtional TSRMLS_DC) /* {{{ */ {
	return 1;
} /* }}} */

yar_transport_interface_t * php_yar_socket_init(TSRMLS_D) /* {{{ */ {
	yar_socket_data_t *data;
	yar_transport_interface_t *self;

	self = ecalloc(1, sizeof(yar_transport_interface_t));
	self->data = data = ecalloc(1, sizeof(yar_socket_data_t));

	self->open   	= php_yar_socket_open;
	self->send   	= php_yar_socket_send;
	self->exec   	= php_yar_socket_exec;
	self->setopt	= php_yar_socket_setopt;
	self->calldata 	= NULL;
	self->close  	= php_yar_socket_close;

	return  self;
} /* }}} */

void php_yar_socket_destroy(yar_transport_interface_t *self TSRMLS_DC) /* {{{ */ {
} /* }}} */

/* {{{ yar_transport_t yar_transport_socket
 */
yar_transport_t yar_transport_socket = {
	"sock",
	php_yar_socket_init,
	php_yar_socket_destroy,
	NULL
}; /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
