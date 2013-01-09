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
#include "yar_protocol.h"
#include "yar_request.h"
#include "yar_response.h"
#include "yar_transport.h"
#include "yar_packager.h"
#include "yar_exception.h"
#include "ext/standard/php_var.h" /* for serialize */
#include "ext/standard/php_smart_str.h" /* for smart string */

#ifdef ENABLE_EPOLL
#include <sys/epoll.h>
#define YAR_EPOLL_MAX_SIZE 128
#endif

#define SEND_BUF_SIZE 1280
#define RECV_BUF_SIZE 1280

typedef struct _yar_socket_data_t {
	char persistent;
	php_stream *stream;
} yar_socket_data_t;

int php_yar_socket_open(yar_transport_interface_t *self, char *address, uint len, long options, char **err_msg TSRMLS_DC) /* {{{ */ {
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;
	struct timeval tv;
	php_stream *stream = NULL;
	char *errstr = NULL, *persistent_key = NULL;
	int err;

	tv.tv_sec = YAR_G(connect_timeout);
	tv.tv_usec = 0;

	if (options & YAR_PROTOCOL_PERSISTENT) {
		data->persistent = 1;
		spprintf(&persistent_key, 0, "yar_%s", address);
	} else {
		data->persistent = 0;
	}

	stream = php_stream_xport_create(address, len, REPORT_ERRORS,
			STREAM_XPORT_CLIENT|STREAM_XPORT_CONNECT, persistent_key, &tv, NULL, &errstr, &err);

	if (persistent_key) {
		efree(persistent_key);
	}

	if (stream == NULL) {
		spprintf(err_msg, 0, "Unable to connect to %s (%s)", address, strerror(errno));
		efree(errstr);
		return 0;
	}

	php_stream_set_option(stream, PHP_STREAM_OPTION_BLOCKING, 0, NULL);

	stream->__exposed++;

	data->stream = stream;
		
	return 1;
} /* }}} */

void php_yar_socket_close(yar_transport_interface_t* self TSRMLS_DC) /* {{{ */ {
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;

	if (!data) {
		return;
	}

	/* stream will be closed by engine
	if (data->stream) {
		php_stream_close(data->stream);
	}
	*/

	efree(data);
	efree(self);

	return;
}
/* }}} */

yar_response_t * php_yar_socket_exec(yar_transport_interface_t* self, yar_request_t *request TSRMLS_DC) /* {{{ */ {
	fd_set rfds;
	struct timeval tv;
	yar_header_t *header;
	yar_response_t *response;
	int fd, retval, recvd;
   	size_t len, total_recvd = 0;
	char *msg, buf[RECV_BUF_SIZE], *payload = NULL;
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;

	response = ecalloc(1, sizeof(yar_response_t));

	FD_ZERO(&rfds);
	if (SUCCESS == php_stream_cast(data->stream, PHP_STREAM_AS_FD_FOR_SELECT | PHP_STREAM_CAST_INTERNAL, (void*)&fd, 1) && fd >= 0) {
		PHP_SAFE_FD_SET(fd, &rfds);
	} else {
		len = snprintf(buf, sizeof(buf), "Unable cast socket fd form stream (%s)", strerror(errno));
		php_yar_response_set_error(response, YAR_ERR_TRANSPORT, buf, len TSRMLS_CC);
		return response;
	}

	tv.tv_sec = 1;
	tv.tv_usec = 0;

wait_io:
	while ((retval = php_select(fd+1, &rfds, NULL, NULL, &tv)) == 0);

	if (retval == -1) {
		len = snprintf(buf, sizeof(buf), "Unable to select %d '%s'", fd, strerror(errno));
		php_yar_response_set_error(response, YAR_ERR_TRANSPORT, buf, len TSRMLS_CC);
		return response;
	}

	if (PHP_SAFE_FD_ISSET(fd, &rfds)) {
		zval *retval;
		if (!payload) {
			if ((recvd = php_stream_xport_recvfrom(data->stream, buf, sizeof(buf), 0, NULL, NULL, NULL, NULL TSRMLS_CC)) > 0) {
				if (!(header = php_yar_protocol_parse(buf, &msg TSRMLS_CC))) {
					php_yar_response_set_error(response, YAR_ERR_PROTOCOL, msg, strlen(msg) TSRMLS_CC);
					return response;
				}

				payload = emalloc(header->body_len);
				len = header->body_len;
				total_recvd  = recvd - sizeof(yar_header_t);

				memcpy(payload, buf + sizeof(yar_header_t), total_recvd);

				if (recvd < (sizeof(yar_header_t) + len)) {
					goto wait_io;	
				}
			} else if (recvd < 0) {
				/* this should never happen */
				goto wait_io;
			}
		} else {
			if ((recvd = php_stream_xport_recvfrom(data->stream, payload + total_recvd, len - total_recvd, 0, NULL, NULL, NULL, NULL TSRMLS_CC)) > 0) {
				total_recvd += recvd;
			}

			if (total_recvd < len) {
				goto wait_io;
			}
		}

		if (len) {
			if (!(retval = php_yar_packager_unpack(payload, len, &msg TSRMLS_CC))) {
				php_yar_response_set_error(response, YAR_ERR_PACKAGER, msg, strlen(msg) TSRMLS_CC);
				return response;
			}

			efree(payload);

			php_yar_response_map_retval(response, retval TSRMLS_CC);
			zval_ptr_dtor(&retval);
		} else {
			php_yar_response_set_error(response, YAR_ERR_EMPTY_RESPONSE, ZEND_STRL("empty response") TSRMLS_CC);
		}
		return response;
	} else {
		goto wait_io;
	}
} /* }}} */

int php_yar_socket_send(yar_transport_interface_t* self, yar_request_t *request, char **msg TSRMLS_DC) /* {{{ */ {
	int ret;
	zval *payload;
	yar_header_t header = {0};
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;
	char buf[SEND_BUF_SIZE];

	if (!(payload = php_yar_request_pack(request, msg TSRMLS_CC))) {
		return 0;
	}

	php_yar_protocol_render(&header, request->id, NULL, NULL, Z_STRLEN_P(payload), data->persistent? YAR_PROTOCOL_PERSISTENT : 0 TSRMLS_CC);

	memcpy(buf, (char *)&header, sizeof(yar_header_t));
	if (Z_STRLEN_P(payload) > (sizeof(buf) - sizeof(yar_header_t))) {
		size_t bytes_left, bytes_sent;

		memcpy(buf + sizeof(yar_header_t), Z_STRVAL_P(payload), sizeof(buf) - sizeof(yar_header_t));
		if (php_stream_xport_sendto(data->stream, buf, sizeof(buf), 0, NULL, 0 TSRMLS_CC) < 0) {
			return 0;
		}
		
		bytes_sent = sizeof(buf) - sizeof(yar_header_t);
		bytes_left = Z_STRLEN_P(payload) - (sizeof(buf) - sizeof(yar_header_t));
		ret = php_stream_xport_sendto(data->stream, Z_STRVAL_P(payload) + bytes_sent, bytes_left, 0, NULL, 0 TSRMLS_CC);
	} else {
		memcpy(buf + sizeof(yar_header_t), Z_STRVAL_P(payload), Z_STRLEN_P(payload));
		ret = php_stream_xport_sendto(data->stream, buf, sizeof(yar_header_t) + Z_STRLEN_P(payload), 0, NULL, 0 TSRMLS_CC);
	}
	
	zval_ptr_dtor(&payload);

	return ret < 0? 0 : 1;
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
