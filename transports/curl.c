/*
  +----------------------------------------------------------------------+
  | Hermes - Light, concurrent RPC framework                             |
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
#include "php_hermes.h"
#include "hermes_transport.h"
#include "hermes_packager.h"
#include "ext/standard/php_var.h" /* for serialize */
#include "ext/standard/php_smart_str.h" /* for smart string */

#include <curl/curl.h>
#include <curl/easy.h>

typedef struct _hermes_curl_data_t {
	CURL		*cp;
	smart_str	buf;
	smart_str	postfield;
	struct curl_slist *headers;
	zval        *calldata;
	hermes_transport_interface_t *next;
} hermes_curl_data_t;

typedef struct _hermes_curl_multi_data_t {
	CURLM *cm;
	hermes_transport_interface_t *chs;
} hermes_curl_multi_data_t;

size_t php_hermes_curl_buf_writer(char *ptr, size_t size, size_t nmemb, void *ctx) /* {{{ */ {
	hermes_curl_data_t *data = (hermes_curl_data_t *) ctx;
	size_t len = size * nmemb;
	smart_str_appendl(&data->buf, ptr, len);

	return len;
} /* }}} */

int php_hermes_curl_open(hermes_transport_interface_t *self, char *address, uint len, char *hostname, long options TSRMLS_DC) /* {{{ */ {
	CURLcode error = CURLE_OK;
	hermes_curl_data_t *data = (hermes_curl_data_t *)self->data;
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

void php_hermes_curl_close(hermes_transport_interface_t* self TSRMLS_DC) /* {{{ */ {
	hermes_curl_data_t *data = (hermes_curl_data_t *)self->data;

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
	self->data = NULL;

	return;
}
/* }}} */

static void php_hermes_curl_prepare(hermes_transport_interface_t* self TSRMLS_DC) /* {{{ */ {
	hermes_curl_data_t *data = (hermes_curl_data_t *)self->data;
	curl_easy_setopt(data->cp, CURLOPT_POSTFIELDS, data->postfield.c);
	curl_easy_setopt(data->cp, CURLOPT_POSTFIELDSIZE, data->postfield.len);
} /* }}} */

int php_hermes_curl_exec(hermes_transport_interface_t* self, char **response, size_t *len TSRMLS_DC) /* {{{ */ {
	CURL *cp;
	char *buf;
	CURLcode ret;
	hermes_curl_data_t *data = (hermes_curl_data_t *)self->data;

	php_hermes_curl_prepare(self TSRMLS_CC);

	ret = curl_easy_perform(data->cp);
	if (ret) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "curl exec failed '%s'", curl_easy_strerror(ret));
		return 0;
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

int php_hermes_curl_send(hermes_transport_interface_t* self, char *payload, size_t len TSRMLS_DC) /* {{{ */ {
	CURL *cp;
	hermes_curl_data_t *data = (hermes_curl_data_t *)self->data;
	cp = data->cp;
	smart_str_appendl(&data->postfield, payload, len);

	return 1;
} /* }}} */

int php_hermes_curl_set_calldata(hermes_transport_interface_t* self, zval *calldata TSRMLS_DC) /* {{{ */ {
	int r;
	hermes_curl_data_t *data = (hermes_curl_data_t *)self->data;
	Z_ADDREF_P(calldata);
	data->calldata = calldata;
} /* }}} */

int php_hermes_curl_reset(hermes_transport_interface_t* self) /* {{{ */ {
	hermes_curl_data_t *data = (hermes_curl_data_t *)self->data;
	curl_easy_reset(data->cp);
	return 1;
} /* }}} */

hermes_transport_interface_t * php_hermes_curl_init(TSRMLS_D) /* {{{ */ {
	size_t newlen;
	hermes_curl_data_t *data;
	hermes_transport_interface_t *self;

	self = ecalloc(1, sizeof(hermes_transport_interface_t));
	self->data = data = ecalloc(1, sizeof(hermes_curl_data_t));

	data->headers = curl_slist_append(data->headers, "Content-Type: application/octet-stream");
	data->headers = curl_slist_append(data->headers, "User-Agent: PHP Hermes Rpc-" HERMES_VERSION);
	data->headers = curl_slist_append(data->headers, "Expect:");
	data->headers = curl_slist_append(data->headers, "Connection: close");

	self->open   	= php_hermes_curl_open;
	self->send   	= php_hermes_curl_send;
	self->exec   	= php_hermes_curl_exec;
	self->reset		= php_hermes_curl_reset;
	self->calldata 	= php_hermes_curl_set_calldata;
	self->close  	= php_hermes_curl_close;


	smart_str_alloc((&data->buf), HERMES_PACKAGER_BUFFER_SIZE /* 1M */, 0);
	smart_str_alloc((&data->postfield), HERMES_PACKAGER_BUFFER_SIZE /* 1M */, 0);

	{
		CURL *cp;
		char ua[128];

		cp = curl_easy_init();
		if (!cp) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "start curl failed");
			return 0;
		}

		data->cp = cp;

		curl_easy_setopt(cp, CURLOPT_WRITEFUNCTION, php_hermes_curl_buf_writer);
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
		curl_easy_setopt(cp, CURLOPT_CONNECTTIMEOUT_MS, HERMES_G(connect_timeout) * 1000);
#else
		curl_easy_setopt(cp, CURLOPT_CONNECTTIMEOUT, HERMES_G(connect_timeout));
#endif
#if LIBCURL_VERSION_NUM > 0x071002
		curl_easy_setopt(cp, CURLOPT_TIMEOUT_MS, HERMES_G(timeout) * 1000);
#else
		curl_easy_setopt(cp, CURLOPT_TIMEOUT, HERMES_G(timeout));
#endif
	}

	return  self;
} /* }}} */

void php_hermes_curl_destroy(hermes_transport_interface_t *self TSRMLS_DC) /* {{{ */ {
	efree(self);
} /* }}} */

int php_hermes_curl_multi_add_handle(hermes_transport_multi_interface_t *self, hermes_transport_interface_t *handle TSRMLS_DC) /* {{{ */ {
	hermes_curl_multi_data_t *multi = (hermes_curl_multi_data_t *)self->data;
	hermes_curl_data_t *data = (hermes_curl_data_t *)handle->data;

	php_hermes_curl_prepare(handle TSRMLS_CC);

	curl_multi_add_handle(multi->cm, data->cp);

	if (multi->chs) {
		data->next = multi->chs;
		multi->chs = handle;
	} else {
		multi->chs = handle;
	}

	return 1;
} /* }}} */

int php_hermes_curl_multi_exec(hermes_transport_multi_interface_t *self, hermes_client_callback *f, void *opaque TSRMLS_DC) /* {{{ */ {
	int running_count, rest_count;
    hermes_curl_multi_data_t *multi = (hermes_curl_multi_data_t *)self->data;

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
				if (msg->msg == CURLMSG_DONE && msg->data.result == CURLE_OK) {
					uint found = 0;
					hermes_transport_interface_t *handle = multi->chs, *q = NULL;

					for (; handle;) {
						if (msg->easy_handle == ((hermes_curl_data_t*)handle->data)->cp) {
							if (q) {
						        ((hermes_curl_data_t *)q->data)->next = ((hermes_curl_data_t*)handle->data)->next;
							} else {
								multi->chs = ((hermes_curl_data_t*)handle->data)->next;
							}
						    curl_multi_remove_handle(multi->cm, ((hermes_curl_data_t*)handle->data)->cp);
							found = 1;
							break;
						}
						q = handle;
						handle = ((hermes_curl_data_t*)handle->data)->next;
					}

					if (found) {
						char *response;
						size_t len;
						long http_code = 200;
						hermes_curl_data_t *data = (hermes_curl_data_t *)handle->data;
						if(curl_easy_getinfo(data->cp, CURLINFO_RESPONSE_CODE, &http_code) == CURLE_OK 
								&& http_code != 200) {
							php_error_docref(NULL TSRMLS_CC, E_WARNING, "server responsed non-2xx code '%ld'", http_code);
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
						f(data->calldata, opaque, response, len TSRMLS_CC);
					} else {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "unexpected trnasport info missed");
					}
				} else if (msg->msg == CURLMSG_DONE) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "unexpect error '%s'", curl_easy_strerror(msg->data.result));
				}
			} while (msg_in_sequence);
			rest_count = running_count;
		}
	}

	return 1;
} /* }}} */

void php_hermes_curl_multi_close(hermes_transport_multi_interface_t *self TSRMLS_DC) /* {{{ */ {
    hermes_curl_multi_data_t *multi = (hermes_curl_multi_data_t *)self->data;

	if (multi->chs) {
		hermes_transport_interface_t *p, *q;
		p = multi->chs;
		for( ; p;) {
			hermes_curl_data_t *data = (hermes_curl_data_t *)p->data;
			q = data->next;
			p->close(p TSRMLS_CC);
			php_hermes_curl_destroy(p);
			curl_multi_remove_handle(multi->cm, data->cp);
			p = q;
		}
	}
	curl_multi_cleanup(multi->cm);
	efree(multi);
	return ;
} /* }}} */

hermes_transport_multi_interface_t * php_hermes_curl_multi_init(TSRMLS_D) /* {{{ */ {
	hermes_transport_multi_interface_t *multi = ecalloc(1, sizeof(hermes_transport_multi_interface_t));
	hermes_curl_multi_data_t *data = ecalloc(1, sizeof(hermes_curl_multi_data_t));

	data->cm = curl_multi_init();

	multi->data		= data;
	multi->add 		= php_hermes_curl_multi_add_handle;
	multi->exec 	= php_hermes_curl_multi_exec;
	multi->close 	= php_hermes_curl_multi_close;

	return multi;
} /* }}} */

/* {{{ hermes_transport_multi_t hermes_transport_curl_multi
 */
hermes_transport_multi_t hermes_transport_curl_multi = {
	php_hermes_curl_multi_init
};
/* }}} */

/* {{{ hermes_transport_t hermes_transport_curl
 */
hermes_transport_t hermes_transport_curl = {
	"curl",
	php_hermes_curl_init,
	php_hermes_curl_destroy,
	&hermes_transport_curl_multi
}; /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
