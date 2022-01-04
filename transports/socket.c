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
#include "win32/time.h"
#endif

#include "php_yar.h"
#include "yar_protocol.h"
#include "yar_request.h"
#include "yar_response.h"
#include "yar_transport.h"
#include "yar_packager.h"
#include "yar_exception.h"
#include "ext/standard/php_var.h" /* for serialize */

#ifdef ENABLE_EPOLL
#include <sys/epoll.h>
#define YAR_EPOLL_MAX_SIZE 128
#endif

#define MAX_BODY_LEN 1024 * 1024 * 10 /* 10 M */

typedef struct _yar_socket_data_t {
	char persistent;
	php_stream *stream;
} yar_socket_data_t;

int php_yar_socket_open(yar_transport_interface_t *self, zend_string *address, long flags, char **err_msg) /* {{{ */ {
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;
	struct timeval tv;
	php_stream *stream = NULL;
	zend_string *errstr = NULL;
	char *persistent_key = NULL;
	void **options = (void**)*err_msg;
	int err;

	if (options && options[YAR_OPT_CONNECT_TIMEOUT]) {
		tv.tv_sec = (zend_ulong)(((zend_ulong)options[YAR_OPT_CONNECT_TIMEOUT]) / 1000);
		tv.tv_usec = (zend_ulong)((((zend_ulong)options[YAR_OPT_CONNECT_TIMEOUT]) % 1000) * 1000);
	} else {
		tv.tv_sec = (zend_ulong)(YAR_G(connect_timeout) / 1000);
		tv.tv_usec = (zend_ulong)((YAR_G(connect_timeout) % 1000) * 1000);
	}

	if (flags & YAR_PROTOCOL_PERSISTENT) {
		data->persistent = 1;
		spprintf(&persistent_key, 0, "yar_%s", ZSTR_VAL(address));
	} else {
		data->persistent = 0;
	}

	stream = php_stream_xport_create(ZSTR_VAL(address), ZSTR_LEN(address), 0, STREAM_XPORT_CLIENT|STREAM_XPORT_CONNECT, persistent_key, &tv, NULL, &errstr, &err);

	if (persistent_key) {
		efree(persistent_key);
	}

	if (stream == NULL) {
		spprintf(err_msg, 0, "Unable to connect to %s (%s)", ZSTR_VAL(address), strerror(errno));
		efree(errstr);
		return 0;
	}

	php_stream_set_option(stream, PHP_STREAM_OPTION_BLOCKING, 0, NULL);

#if ZEND_DEBUG
	stream->__exposed++;
#endif

	data->stream = stream;
		
	return 1;
} /* }}} */

void php_yar_socket_close(yar_transport_interface_t* self) /* {{{ */ {
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;

	if (!data) {
		return;
	}

	if (!data->persistent && data->stream) {
		php_stream_close(data->stream);
	}

	efree(data);
	efree(self);

	return;
}
/* }}} */

yar_response_t * php_yar_socket_exec(yar_transport_interface_t* self, yar_request_t *request) /* {{{ */ {
	fd_set rfds;
	struct timeval tv;
	yar_header_t *header;
	yar_response_t *response;
	int fd, retval, recvd;
   	size_t len = 0, total_recvd = 0;
	char *msg, buf[RECV_BUF_SIZE], *payload = NULL;
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;

	response = ecalloc(1, sizeof(yar_response_t));

	FD_ZERO(&rfds);
	if (SUCCESS == php_stream_cast(data->stream, PHP_STREAM_AS_FD_FOR_SELECT | PHP_STREAM_CAST_INTERNAL, (void*)&fd, 1) && fd >= 0) {
		PHP_SAFE_FD_SET(fd, &rfds);
	} else {
		len = snprintf(buf, sizeof(buf), "Unable cast socket fd form stream (%s)", strerror(errno));
		php_yar_response_set_error(response, YAR_ERR_TRANSPORT, buf, len);
		return response;
	}

	if (request->options && request->options[YAR_OPT_TIMEOUT]) {
		tv.tv_sec = (zend_ulong)(((zend_ulong)request->options[YAR_OPT_TIMEOUT]) / 1000);
		tv.tv_usec = (zend_ulong)((((zend_ulong)request->options[YAR_OPT_TIMEOUT]) % 1000) * 1000);
	} else {
		tv.tv_sec = (zend_ulong)(YAR_G(timeout) / 1000);
		tv.tv_usec = (zend_ulong)((YAR_G(timeout) % 1000) * 1000);
	}

wait_io:
	retval = php_select(fd+1, &rfds, NULL, NULL, &tv);

	if (retval == -1) {
		len = snprintf(buf, sizeof(buf), "Unable to select %d '%s'", fd, strerror(errno));
		php_yar_response_set_error(response, YAR_ERR_TRANSPORT, buf, len);
		return response;
	} else if (retval == 0) {
		len = snprintf(buf, sizeof(buf), "select timeout %ldms reached", YAR_G(timeout));
		php_yar_response_set_error(response, YAR_ERR_TRANSPORT, buf, len);
		return response;
	}

	if (PHP_SAFE_FD_ISSET(fd, &rfds)) {
		zval *retval, ret;
		if (!payload) {
			if ((recvd = (php_stream_xport_recvfrom(data->stream, buf, sizeof(buf), 0, NULL, NULL, NULL))) > 0) {
				if (recvd < sizeof(yar_header_t)) {
					php_yar_error(response, YAR_ERR_PROTOCOL, "malformed response header, insufficient bytes recieved");
					return response;
				}
				if (!(header = php_yar_protocol_parse(buf))) {
					php_yar_error(response, YAR_ERR_PROTOCOL, "malformed response header '%.32s'", payload);
					return response;
				}
				
				if (header->body_len > MAX_BODY_LEN) {
					php_yar_error(response, YAR_ERR_PROTOCOL, "response body too large %u", header->body_len);
					return response;
				}

				payload = emalloc(header->body_len);
				len = header->body_len;
				total_recvd  = recvd - sizeof(yar_header_t);

				memcpy(payload, buf + sizeof(yar_header_t), total_recvd);

				if (recvd < (sizeof(yar_header_t) + len)) {
					goto wait_io;	
				}
			} else if (recvd == 0) {
				php_yar_response_set_error(response, YAR_ERR_TRANSPORT, ZEND_STRL("server closed connection prematurely"));
				return response;
			} else {
				/* this should never happen */
				goto wait_io;
			}
		} else {
			if ((recvd = php_stream_xport_recvfrom(data->stream, payload + total_recvd, len - total_recvd, 0, NULL, NULL, NULL)) > 0) {
				total_recvd += recvd;
			} else if (recvd == 0) {
				php_yar_response_set_error(response, YAR_ERR_TRANSPORT, ZEND_STRL("server closed connection prematurely"));
				efree(payload);
				return response;
			}
			if (total_recvd < len) {
				goto wait_io;
			}
		}

		if (len) {
			if (!(retval = php_yar_packager_unpack(payload, len, &msg, &ret))) {
				php_yar_response_set_error(response, YAR_ERR_PACKAGER, msg, strlen(msg));
				efree(payload);
				efree(msg);
				return response;
			}

			php_yar_response_map_retval(response, retval);

			DEBUG_C(ZEND_ULONG_FMT": server response content packaged by '%.*s', len '%ld', content '%.32s'",
					response->id, 7, payload, header->body_len, payload + 8);

			efree(payload);
			zval_ptr_dtor(retval);
		} else {
			php_yar_response_set_error(response, YAR_ERR_EMPTY_RESPONSE, ZEND_STRL("empty response"));
		}
		return response;
	} else {
		PHP_SAFE_FD_SET(fd, &rfds);
		goto wait_io;
	}
} /* }}} */

int php_yar_socket_send(yar_transport_interface_t* self, yar_request_t *request, char **msg) /* {{{ */ {
	fd_set rfds;
	zend_string *payload;
	struct timeval tv;
	int ret = -1, fd, retval;
	char buf[SEND_BUF_SIZE];
	yar_header_t header = {0};
	yar_socket_data_t *data = (yar_socket_data_t *)self->data;
	size_t bytes_left = 0, bytes_sent = 0;
	char *provider, *token;

	FD_ZERO(&rfds);
	if (SUCCESS == php_stream_cast(data->stream, PHP_STREAM_AS_FD_FOR_SELECT|PHP_STREAM_CAST_INTERNAL, (void*)&fd, 1) && fd >= 0) {
		PHP_SAFE_FD_SET(fd, &rfds);
	} else {
		spprintf(msg, 0, "Unable cast socket fd form stream (%s)", strerror(errno));
		return 0;
	}

	if (!(payload = php_yar_request_pack(request, msg))) {
		return 0;
	}

	DEBUG_C(ZEND_ULONG_FMT": pack request by '%.*s', result len '%ld', content: '%.32s'", 
			request->id, 7, ZSTR_VAL(payload), ZSTR_LEN(payload), ZSTR_VAL(payload) + 8);

	if (request->options && request->options[YAR_OPT_PROVIDER]) {
		provider = (char*)(ZSTR_VAL((zend_string*)request->options[YAR_OPT_PROVIDER]));
	} else {
		provider = "Yar TCP Client";
	}

	if (request->options && request->options[YAR_OPT_TOKEN]) {
		token = (char*)ZSTR_VAL((zend_string*)request->options[YAR_OPT_TOKEN]);
	} else {
		token = NULL;
	}

	/* for tcp/unix RPC, we need another way to supports auth */
	php_yar_protocol_render(&header, request->id, provider, token, ZSTR_LEN(payload), data->persistent? YAR_PROTOCOL_PERSISTENT : 0);

	memcpy(buf, (char *)&header, sizeof(yar_header_t));

	if (request->options && request->options[YAR_OPT_TIMEOUT]) {
		tv.tv_sec = (zend_ulong)(((zend_ulong)request->options[YAR_OPT_TIMEOUT]) / 1000);
		tv.tv_usec = (zend_ulong)((((zend_ulong)request->options[YAR_OPT_TIMEOUT]) % 1000) * 1000);
	} else {
		tv.tv_sec = (zend_ulong)(YAR_G(timeout) / 1000);
		tv.tv_usec = (zend_ulong)((YAR_G(timeout) % 1000) * 1000);
	}

	retval = php_select(fd+1, NULL, &rfds, NULL, &tv);

	if (retval == -1) {
		zend_string_release(payload);
		spprintf(msg, 0, "select error '%s'", strerror(errno));
		return 0;
	} else if (retval == 0) {
		zend_string_release(payload);
		spprintf(msg, 0, "select timeout %ldms reached", YAR_G(timeout));
		return 0;
	}

	if (PHP_SAFE_FD_ISSET(fd, &rfds)) {
		if (ZSTR_LEN(payload) > (sizeof(buf) - sizeof(yar_header_t))) {
			memcpy(buf + sizeof(yar_header_t), ZSTR_VAL(payload), sizeof(buf) - sizeof(yar_header_t));
			if ((ret = php_stream_xport_sendto(data->stream, buf, sizeof(buf), 0, NULL, 0)) < 0) {
				zend_string_release(payload);
				spprintf(msg, 0, "unable to send data");
				return 0;
			}
		} else {
			memcpy(buf + sizeof(yar_header_t), ZSTR_VAL(payload), ZSTR_LEN(payload));
			if ((ret = php_stream_xport_sendto(data->stream, buf, sizeof(yar_header_t) + ZSTR_LEN(payload), 0, NULL, 0)) < 0) {
				zend_string_release(payload);
				spprintf(msg, 0, "unable to send data");
				return 0;
			}
		}

		bytes_sent = ret - sizeof(yar_header_t);
		bytes_left = ZSTR_LEN(payload) - bytes_sent;

wait_io:
		if (bytes_left) {
			retval = php_select(fd+1, NULL, &rfds, NULL, &tv);

			if (retval == -1) {
				zend_string_release(payload);
				spprintf(msg, 0, "select error '%s'", strerror(errno));
				return 0;
			} else if (retval == 0) {
				zend_string_release(payload);
				spprintf(msg, 0, "select timeout %ldms reached", YAR_G(timeout));
				return 0;
			}

			if (PHP_SAFE_FD_ISSET(fd, &rfds)) {
				if ((ret = php_stream_xport_sendto(data->stream, ZSTR_VAL(payload) + bytes_sent, bytes_left, 0, NULL, 0)) > 0) {
					bytes_left -= ret;
					bytes_sent += ret;
				}
			}
			goto wait_io;
		}
	} else {
		PHP_SAFE_FD_SET(fd, &rfds);
		goto wait_io;
	}

	if (!data->persistent) {
		php_stream_xport_shutdown(data->stream, SHUT_WR);
	}

	zend_string_release(payload);

	return ret < 0? 0 : 1;
} /* }}} */

int php_yar_socket_setopt(yar_transport_interface_t* self, long type, void *value, void *addtional) /* {{{ */ {
	return 1;
} /* }}} */

yar_transport_interface_t * php_yar_socket_init() /* {{{ */ {
	yar_socket_data_t *data;
	yar_transport_interface_t *self;

	self = emalloc(sizeof(yar_transport_interface_t));
	self->data = data = ecalloc(1, sizeof(yar_socket_data_t));

	self->open   	= php_yar_socket_open;
	self->send   	= php_yar_socket_send;
	self->exec   	= php_yar_socket_exec;
	self->setopt	= php_yar_socket_setopt;
	self->calldata 	= NULL;
	self->close  	= php_yar_socket_close;

	return  self;
} /* }}} */

void php_yar_socket_destroy(yar_transport_interface_t *self) /* {{{ */ {
} /* }}} */

/* {{{ yar_transport_t yar_transport_socket
 */
const yar_transport_t yar_transport_socket = {
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
