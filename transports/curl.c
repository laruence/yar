/*
  +----------------------------------------------------------------------+
  | Yar - Light, concurrent RPC framework                             |
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
#include "php_yar.h"
#include "yar_transport.h"
#include "yar_packager.h"
#include "ext/standard/php_var.h" /* for serialize */
#include "ext/standard/php_smart_str.h" /* for smart string */

#include <curl/curl.h>
#include <curl/easy.h>

typedef struct _yar_curl_data_t {
	CURL		*cp;
	smart_str	buf;
	smart_str	postfield;
	struct curl_slist *headers;
	zval        *calldata;
	yar_transport_interface_t *next;
} yar_curl_data_t;

typedef struct _yar_curl_multi_data_t {
	CURLM *cm;
	yar_transport_interface_t *chs;
} yar_curl_multi_data_t;

size_t php_yar_curl_buf_writer(char *ptr, size_t size, size_t nmemb, void *ctx) /* {{{ */ {
	yar_curl_data_t *data = (yar_curl_data_t *) ctx;
	size_t len = size * nmemb;
	smart_str_appendl(&data->buf, ptr, len);

	return len;
} /* }}} */

int php_yar_curl_open(yar_transport_interface_t *self, char *address, uint len, char *hostname, long options TSRMLS_DC) /* {{{ */ {
	CURLcode error = CURLE_OK;
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;
	char buf[1024];

	snprintf(buf, sizeof(buf), "Hostname: %s", hostname);
	buf[sizeof(buf) - 1] = '\0';
	data->headers = curl_slist_append(data->headers, buf);
	curl_easy_setopt(data->cp, CURLOPT_HTTPHEADER, data->headers);

#if LIBCURL_VERSION_NUM >= 0x071100
	error = curl_easy_setopt(data->cp, CURLOPT_URL, address);
#else
	error = curl_easy_setopt(data->cp, CURLOPT_URL, estrndup(address, len));
#endif

	return (error == CURLE_OK ? 1 : 0);
} /* }}} */

void php_yar_curl_close(yar_transport_interface_t* self TSRMLS_DC) /* {{{ */ {
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;

	if (!data) {
		return;
	}

	if (data->cp) {
		curl_easy_cleanup(data->cp);
		data->cp = NULL;
	}

	if (data->calldata) {
		zval_ptr_dtor(&data->calldata);
	}

	smart_str_free(&data->buf);
	smart_str_free(&data->postfield);
	curl_slist_free_all(data->headers);

	efree(data);
	efree(self);

	return;
}
/* }}} */

static void php_yar_curl_prepare(yar_transport_interface_t* self TSRMLS_DC) /* {{{ */ {
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;
	curl_easy_setopt(data->cp, CURLOPT_POSTFIELDS, data->postfield.c);
	curl_easy_setopt(data->cp, CURLOPT_POSTFIELDSIZE, data->postfield.len);
} /* }}} */

int php_yar_curl_exec(yar_transport_interface_t* self, char **response, size_t *len, uint *code, char **msg TSRMLS_DC) /* {{{ */ {
	CURL *cp;
	char *buf;
	CURLcode ret;
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;

	php_yar_curl_prepare(self TSRMLS_CC);

	ret = curl_easy_perform(data->cp);
	if (ret != CURLE_OK) {
		spprintf(msg, 0, "curl exec failed '%s'", curl_easy_strerror(ret));
		*code = ret;
		return 0;
	} else {
		long http_code;
		if(curl_easy_getinfo(data->cp, CURLINFO_RESPONSE_CODE, &http_code) == CURLE_OK 
				&& http_code != 200) {
			spprintf(msg, 0, "server responsed non-200 code '%ld'", http_code);
			*code = http_code;
			return 0;
		}
	}

	if (data->buf.a) {
		smart_str_0(&data->buf);
		*response = data->buf.c;
		*len = data->buf.len;

		data->buf.c = NULL;
		data->buf.a = 0;
		data->buf.len = 0;
	} else {
		*response = NULL;
		*len = 0;
	}
	return 1;
} /* }}} */

int php_yar_curl_send(yar_transport_interface_t* self, char *payload, size_t len TSRMLS_DC) /* {{{ */ {
	CURL *cp;
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;
	cp = data->cp;
	smart_str_appendl(&data->postfield, payload, len);

	return 1;
} /* }}} */

int php_yar_curl_set_calldata(yar_transport_interface_t* self, zval *calldata TSRMLS_DC) /* {{{ */ {
	int r;
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;
	Z_ADDREF_P(calldata);
	data->calldata = calldata;
} /* }}} */

int php_yar_curl_reset(yar_transport_interface_t* self) /* {{{ */ {
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;
	curl_easy_reset(data->cp);
	return 1;
} /* }}} */

yar_transport_interface_t * php_yar_curl_init(TSRMLS_D) /* {{{ */ {
	size_t newlen;
	yar_curl_data_t *data;
	yar_transport_interface_t *self;

	self = ecalloc(1, sizeof(yar_transport_interface_t));
	self->data = data = ecalloc(1, sizeof(yar_curl_data_t));

	data->headers = curl_slist_append(data->headers, "Content-Type: application/octet-stream");
	data->headers = curl_slist_append(data->headers, "User-Agent: PHP Yar Rpc-" YAR_VERSION);
	data->headers = curl_slist_append(data->headers, "Expect:");
	data->headers = curl_slist_append(data->headers, "Connection: close");

	self->open   	= php_yar_curl_open;
	self->send   	= php_yar_curl_send;
	self->exec   	= php_yar_curl_exec;
	self->reset		= php_yar_curl_reset;
	self->calldata 	= php_yar_curl_set_calldata;
	self->close  	= php_yar_curl_close;


	smart_str_alloc((&data->buf), YAR_PACKAGER_BUFFER_SIZE /* 1M */, 0);
	smart_str_alloc((&data->postfield), YAR_PACKAGER_BUFFER_SIZE /* 1M */, 0);

	{
		CURL *cp;
		char ua[128];

		cp = curl_easy_init();
		if (!cp) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "start curl failed");
			return 0;
		}

		data->cp = cp;

		curl_easy_setopt(cp, CURLOPT_WRITEFUNCTION, php_yar_curl_buf_writer);
		curl_easy_setopt(cp, CURLOPT_WRITEDATA, self->data);
		curl_easy_setopt(cp, CURLOPT_NETRC, 0);
		curl_easy_setopt(cp, CURLOPT_HEADER, 0);
		curl_easy_setopt(cp, CURLOPT_VERBOSE, 0);
		curl_easy_setopt(cp, CURLOPT_FOLLOWLOCATION, 0);
		curl_easy_setopt(cp, CURLOPT_SSL_VERIFYHOST, 0);
		curl_easy_setopt(cp, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(cp, CURLOPT_POST, 1);
		curl_easy_setopt(cp, CURLOPT_NOPROGRESS, 1);
#if defined(ZTS)
		curl_easy_setopt(cp, CURLOPT_NOSIGNAL, 1);
#endif
		curl_easy_setopt(cp, CURLOPT_DNS_USE_GLOBAL_CACHE, 1);
		curl_easy_setopt(cp, CURLOPT_DNS_CACHE_TIMEOUT, 180);
		curl_easy_setopt(cp, CURLOPT_TCP_NODELAY, 0);
		curl_easy_setopt(cp, CURLOPT_IGNORE_CONTENT_LENGTH, 1);
#if LIBCURL_VERSION_NUM > 0x071002
		curl_easy_setopt(cp, CURLOPT_CONNECTTIMEOUT_MS, YAR_G(connect_timeout) * 1000);
#else
		curl_easy_setopt(cp, CURLOPT_CONNECTTIMEOUT, YAR_G(connect_timeout));
#endif
#if LIBCURL_VERSION_NUM > 0x071002
		curl_easy_setopt(cp, CURLOPT_TIMEOUT_MS, YAR_G(timeout) * 1000);
#else
		curl_easy_setopt(cp, CURLOPT_TIMEOUT, YAR_G(timeout));
#endif
	}

	return  self;
} /* }}} */

void php_yar_curl_destroy(yar_transport_interface_t *self TSRMLS_DC) /* {{{ */ {
} /* }}} */

int php_yar_curl_multi_add_handle(yar_transport_multi_interface_t *self, yar_transport_interface_t *handle TSRMLS_DC) /* {{{ */ {
	yar_curl_multi_data_t *multi = (yar_curl_multi_data_t *)self->data;
	yar_curl_data_t *data = (yar_curl_data_t *)handle->data;

	php_yar_curl_prepare(handle TSRMLS_CC);

	curl_multi_add_handle(multi->cm, data->cp);

	if (multi->chs) {
		data->next = multi->chs;
		multi->chs = handle;
	} else {
		multi->chs = handle;
	}

	return 1;
} /* }}} */

int php_yar_curl_multi_exec(yar_transport_multi_interface_t *self, yar_concurrent_client_callback *f, yar_concurrent_client_error_callback *ef, void *opaque1, void *opaque2 TSRMLS_DC) /* {{{ */ {
	int running_count, rest_count;
    yar_curl_multi_data_t *multi = (yar_curl_multi_data_t *)self->data;

	while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multi->cm, &running_count));

    rest_count = running_count;
	while (running_count) {
		int max_fd, return_code;
		struct timeval tv;
		fd_set readfds;
		fd_set writefds;
		fd_set exceptfds;

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);

		curl_multi_fdset(multi->cm, &readfds, &writefds, &exceptfds, &max_fd);
		return_code = select(max_fd + 1, &readfds, &writefds, &exceptfds, &tv);
		if (-1 == return_code) {
			return 0;
		} else {
			while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multi->cm, &running_count));
		}

        if (rest_count > running_count) {
			int msg_in_sequence;
			CURLMsg *msg;

			do {
				msg = curl_multi_info_read(multi->cm, &msg_in_sequence);
				if (msg->msg == CURLMSG_DONE) {
					uint found = 0;
					yar_transport_interface_t *handle = multi->chs, *q = NULL;

					for (; handle;) {
						if (msg->easy_handle == ((yar_curl_data_t*)handle->data)->cp) {
							if (q) {
						        ((yar_curl_data_t *)q->data)->next = ((yar_curl_data_t*)handle->data)->next;
							} else {
								multi->chs = ((yar_curl_data_t*)handle->data)->next;
							}
							curl_multi_remove_handle(multi->cm, ((yar_curl_data_t*)handle->data)->cp);
							found = 1;
							break;
						}
						q = handle;
						handle = ((yar_curl_data_t*)handle->data)->next;
					}

					if (found) {
						char *response;
						size_t len;
						long http_code = 200;
						yar_curl_data_t *data = (yar_curl_data_t *)handle->data;
						if (msg->data.result == CURLE_OK) {
							if(curl_easy_getinfo(data->cp, CURLINFO_RESPONSE_CODE, &http_code) == CURLE_OK 
									&& http_code != 200) {
								if (ef) {
									if (!ef(data->calldata, opaque2, http_code, (char *)"server response non-200 code" TSRMLS_CC)) {
										handle->close(handle TSRMLS_CC);
										zend_bailout();
									}
									if (EG(exception)) {
										handle->close(handle TSRMLS_CC);
										return 0;
									}
								} else {
									php_error_docref(NULL TSRMLS_CC, E_WARNING, "server responsed non-200 code '%ld'", http_code);
								}
								handle->close(handle TSRMLS_CC);
								continue;
							}
							if (data->buf.a) {
								smart_str_0(&data->buf);
								response = data->buf.c;
								len = data->buf.len;

								data->buf.c = NULL;
								data->buf.a = 0;
								data->buf.len = 0;
							} else {
								response = NULL;
								len = 0;
							}
							if (!f(data->calldata, opaque1, response, len TSRMLS_CC)) {
								handle->close(handle TSRMLS_CC);
								zend_bailout();
							}
							if (EG(exception)) {
								handle->close(handle TSRMLS_CC);
								return 0;
							}
						} else if (ef) {
							if (!ef(data->calldata, opaque2, msg->data.result, curl_easy_strerror(msg->data.result) TSRMLS_CC)) {
								handle->close(handle TSRMLS_CC);
								zend_bailout();
							}
							if (EG(exception)) {
								handle->close(handle TSRMLS_CC);
								return 0;
							}
						} else {
							php_error_docref(NULL TSRMLS_CC, E_WARNING, "unexpect error '%s'", curl_easy_strerror(msg->data.result));
						}
						handle->close(handle TSRMLS_CC);
					} else {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "unexpected trnasport info missed");
					}
				} 
			} while (msg_in_sequence);
			rest_count = running_count;
		}
	}

	return 1;
} /* }}} */

void php_yar_curl_multi_close(yar_transport_multi_interface_t *self TSRMLS_DC) /* {{{ */ {
    yar_curl_multi_data_t *multi = (yar_curl_multi_data_t *)self->data;

	if (multi->chs) {
		yar_transport_interface_t *p, *q;
		p = multi->chs;
		for( ; p;) {
			yar_curl_data_t *data = (yar_curl_data_t *)p->data;
			q = data->next;
			curl_multi_remove_handle(multi->cm, data->cp);
			p->close(p TSRMLS_CC);
			p = q;
		}
	}
	curl_multi_cleanup(multi->cm);
	efree(multi);
	efree(self);
	return ;
} /* }}} */

yar_transport_multi_interface_t * php_yar_curl_multi_init(TSRMLS_D) /* {{{ */ {
	yar_transport_multi_interface_t *multi = ecalloc(1, sizeof(yar_transport_multi_interface_t));
	yar_curl_multi_data_t *data = ecalloc(1, sizeof(yar_curl_multi_data_t));

	data->cm = curl_multi_init();

	multi->data		= data;
	multi->add 		= php_yar_curl_multi_add_handle;
	multi->exec 	= php_yar_curl_multi_exec;
	multi->close 	= php_yar_curl_multi_close;

	return multi;
} /* }}} */

/* {{{ yar_transport_multi_t yar_transport_curl_multi
 */
yar_transport_multi_t yar_transport_curl_multi = {
	php_yar_curl_multi_init
};
/* }}} */

/* {{{ yar_transport_t yar_transport_curl
 */
yar_transport_t yar_transport_curl = {
	"curl",
	php_yar_curl_init,
	php_yar_curl_destroy,
	&yar_transport_curl_multi
}; /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
