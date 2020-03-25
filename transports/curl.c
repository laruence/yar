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

#ifdef PHP_WIN32
#include "win32/time.h"
#endif

#include "php.h"
#include "php_yar.h"
#include "yar_protocol.h"
#include "yar_response.h"
#include "yar_request.h"
#include "yar_transport.h"
#include "yar_packager.h"
#include "yar_exception.h"
#include "ext/standard/php_var.h" /* for serialize */
#include "ext/standard/url.h" /* for php_url */

#include <curl/curl.h>
#include <curl/easy.h>

#ifdef ENABLE_EPOLL
#include <sys/epoll.h>
#define YAR_EPOLL_MAX_SIZE 64
#include <sys/timerfd.h>
#endif

typedef struct _yar_curl_plink {
	CURL *cp;
	char in_use;
	struct _yar_curl_plink *next;
} yar_curl_plink_t;

typedef struct _yar_curl_data_t {
	CURL		*cp;
	char 		persistent;
	smart_str	buf;
	smart_str	postfield;
	php_url     *host;
	yar_call_data_t *calldata;
	yar_curl_plink_t *plink;
	struct curl_slist *headers;
	yar_transport_interface_t *next;
#if LIBCURL_VERSION_NUM < 0x071100
	zend_string *address;
#endif
} yar_curl_data_t;

typedef struct _yar_curl_multi_data_t {
	CURLM *cm;
	yar_transport_interface_t *chs;
} yar_curl_multi_data_t;

#ifdef ENABLE_EPOLL
typedef struct _yar_curl_multi_gdata {
	int epfd;
	int tfd;
	CURLM *cm;
} yar_curl_multi_gdata;

typedef struct _yar_curl_multi_sockinfo {
	curl_socket_t fd;
	CURL *cp;
} yar_curl_multi_sockinfo;

static int php_yar_start_timer(yar_curl_multi_gdata *g) /* {{{ */ {
	struct epoll_event ev = {0};
	struct itimerspec its = {0};

	g->tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

	if (g->tfd == -1) {
		php_error_docref(NULL, E_WARNING, "timerfd_create failed '%s'", strerror(errno));
		close(g->epfd);
		return 0;
	}
	its.it_value.tv_nsec = 1;
	timerfd_settime(g->tfd, 0, &its, NULL);

	ev.events = EPOLLIN;
	ev.data.fd = g->tfd;
	epoll_ctl(g->epfd, EPOLL_CTL_ADD, g->tfd, &ev);
	return 1;
}
/* }}} */

static int php_yar_timer_cb(CURLM *cm, long timeout_ms, void *cbp) /* {{{ */ {
	struct itimerspec its = {0};
	yar_curl_multi_gdata *g = (yar_curl_multi_gdata *) cbp;

	if (timeout_ms > 0) {
		its.it_value.tv_sec = timeout_ms / 1000;
		its.it_value.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
	} else if (timeout_ms == 0) {
		/* libcurl wants us to timeout now, however setting both fields of
		 * new_value.it_value to zero disarms the timer. The closest we can
		 * do is to schedule the timer to fire in 1 ns. */
		its.it_value.tv_nsec = 1;
	}

	timerfd_settime(g->tfd, 0, &its, NULL);
	return 0;
}
/* }}} */

static int php_yar_sock_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp) /* {{{ */ {
	struct epoll_event ev = {0};
	yar_curl_multi_gdata *g = (yar_curl_multi_gdata *) cbp;
	yar_curl_multi_sockinfo *fdp = (yar_curl_multi_sockinfo *) sockp;
	/* const char *whatstr[]={ "none", "IN", "OUT", "INOUT", "REMOVE" };
	fprintf(stderr, "socket callback: s=%d e=%p what=%s\n", s, e, whatstr[what]); */

	ev.data.fd = s;
	if (what == CURL_POLL_REMOVE) {
		if (fdp) {
			efree(fdp);
		}
		epoll_ctl(g->epfd, EPOLL_CTL_DEL, s, &ev);
	} else {
		/* EPOLLET does not work normally with libcurl */
		if (what == CURL_POLL_IN) {
			ev.events |= EPOLLIN;
		} else if (what == CURL_POLL_OUT) {
			ev.events |= EPOLLOUT;
		} else if (what == CURL_POLL_INOUT) {
			ev.events |= EPOLLIN | EPOLLOUT;
		}
		if (!fdp) {
			fdp = emalloc(sizeof(yar_curl_multi_sockinfo));
			fdp->fd = s;
			fdp->cp = e;

			epoll_ctl(g->epfd, EPOLL_CTL_ADD, s, &ev);	
			curl_multi_assign(g->cm, s, fdp);
		} else {
			epoll_ctl(g->epfd, EPOLL_CTL_MOD, s, &ev);	
		}
	}
	return 0;
} /* }}} */
#endif

void php_yar_curl_plink_dtor(void *ptr) /* {{{ */ {
	yar_curl_plink_t *p, *q;
	for (p = (yar_curl_plink_t *)ptr; p;) {
		q = p->next;
		curl_easy_cleanup((CURL *)p->cp);
		efree(p);
		p = q;
	}
}
/* }}} */

size_t php_yar_curl_buf_writer(char *ptr, size_t size, size_t nmemb, void *ctx) /* {{{ */ {
	yar_curl_data_t *data = (yar_curl_data_t *) ctx;
	size_t len = size * nmemb;
	smart_str_appendl(&data->buf, ptr, len);

	return len;
} /* }}} */

int php_yar_curl_open(yar_transport_interface_t *self, zend_string *address, long options, char **msg) /* {{{ */ {
	CURL *cp = NULL;
	php_url *url;
	char buf[1024];
	CURLcode error = CURLE_OK;
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;
	zval *configs = (zval *)*msg;

	if (options & YAR_PROTOCOL_PERSISTENT) {
		zend_resource *le;
		size_t key_len = snprintf(buf, sizeof(buf), "yar_%s", ZSTR_VAL(address));

		data->persistent = 1;
		if ((le = zend_hash_str_find_ptr(&EG(regular_list), buf, key_len)) == NULL) {
			yar_persistent_le_t *con;
			yar_curl_plink_t *plink;
			zend_resource new_le;

			cp = curl_easy_init();
			if (!cp) {
				php_error_docref(NULL, E_ERROR, "start curl failed");
				return 0;
			}

			plink = emalloc(sizeof(yar_curl_plink_t));
			if (!plink) {
				goto regular_link;
			}

			con = emalloc(sizeof(yar_persistent_le_t));
			if (!con) {
				efree(plink);
				goto regular_link;
			}

			plink->cp = cp;
			plink->in_use = 1;
			plink->next = NULL;

			con->ptr = plink;
			con->dtor = php_yar_curl_plink_dtor;

			new_le.type = le_plink;
			new_le.ptr = con;

			if (zend_hash_str_update_mem(&EG(regular_list), buf, key_len, (void *)&new_le, sizeof(new_le)) != NULL) {
				data->plink = plink;
				data->cp = plink->cp;
			} else {
				data->persistent = 0;
				efree(plink);
			}
		} else {
			yar_curl_plink_t *plink;
			yar_persistent_le_t *con = (yar_persistent_le_t *)le->ptr;

			for (plink = (yar_curl_plink_t *)con->ptr; plink; plink = plink->next) {
				if (!plink->in_use) {
					cp = (CURL *)plink->cp;
					curl_easy_reset(cp);
					plink->in_use = 1;
					data->plink = plink;
					break;
				}
			}

			if (!cp) {
				cp = curl_easy_init();
				if (!cp) {
					php_error_docref(NULL, E_ERROR, "start curl failed");
					return 0;
				}

				plink = emalloc(sizeof(yar_curl_plink_t));
				if (plink) {
					plink->next = (yar_curl_plink_t *)con->ptr;
					plink->in_use = 1;
					plink->cp = cp;
					con->ptr = plink;
					data->plink = plink;
				} else {
					data->persistent = 0;
				}
			}
		}
	} else {
regular_link:
		cp = curl_easy_init();
		if (!cp) {
			php_error_docref(NULL, E_ERROR, "start curl failed");
			return 0;
		}
	}

	if (!(url = php_url_parse(ZSTR_VAL(address)))) {
		spprintf(msg, 0, "malformed uri: '%s'", ZSTR_VAL(address));
		return 0;
	}

	data->host = url;
	data->cp = cp;

	if (data->persistent) {
		data->headers = curl_slist_append(data->headers, "Connection: Keep-Alive");
		data->headers = curl_slist_append(data->headers, "Keep-Alive: 300");
	} else {
		data->headers = curl_slist_append(data->headers, "Connection: close");
	}

#if PHP_VERSION_ID < 70300
	snprintf(buf, sizeof(buf), "Hostname: %s", url->host);
#else
	snprintf(buf, sizeof(buf), "Hostname: %s", ZSTR_VAL(url->host));
#endif
	buf[sizeof(buf) - 1] = '\0';
	data->headers = curl_slist_append(data->headers, buf);

	if (configs && IS_ARRAY == Z_TYPE_P(configs)) {
		zval *resolve, *headers;

		if ((headers = zend_hash_index_find(Z_ARRVAL_P(configs), YAR_OPT_HEADER))) {
			zval *val;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(headers), val) {
				if (Z_TYPE_P(val) != IS_STRING) continue;
				data->headers = curl_slist_append(data->headers, Z_STRVAL_P(val));
			}ZEND_HASH_FOREACH_END();
		}
		if ((resolve = zend_hash_index_find(Z_ARRVAL_P(configs), YAR_OPT_RESOLVE))) {
#if LIBCURL_VERSION_NUM >= 0x071503 /* Available since 7.21.3 */
			zval *val;
			struct curl_slist *slist = NULL;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(resolve), val) {
				if (Z_TYPE_P(val) != IS_STRING) continue;
				slist = curl_slist_append(slist, Z_STRVAL_P(val));
			} ZEND_HASH_FOREACH_END();
			curl_easy_setopt(data->cp, CURLOPT_RESOLVE, slist);
#endif
		}
	}

	curl_easy_setopt(data->cp, CURLOPT_HTTPHEADER, data->headers);

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
#if LIBCURL_VERSION_NUM > 0x070A00
	/* For the bug that when libcurl use standard name resolver, 
	 * Any timeout less than 1000ms will cause libcurl return timeout immediately
	 * Added in 7.10 */
	curl_easy_setopt(cp, CURLOPT_NOSIGNAL, 1);
#endif


	curl_easy_setopt(cp, CURLOPT_DNS_USE_GLOBAL_CACHE, 1);
	/* let's cache the DNS result 5 mins */
	curl_easy_setopt(cp, CURLOPT_DNS_CACHE_TIMEOUT, 300);
	curl_easy_setopt(cp, CURLOPT_TCP_NODELAY, 0);

	if (!data->persistent) {
#if LIBCURL_VERSION_NUM > 0x070E01
		/* Added in 7.14.1 */
		curl_easy_setopt(cp, CURLOPT_IGNORE_CONTENT_LENGTH, 1);
#endif
	} else {
#if LIBCURL_VERSION_NUM >= 0x072100
		/* Available since 7.33.0 */
		/* curl_easy_setopt(cp, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0); */
#endif
	}

#if LIBCURL_VERSION_NUM > 0x071002
	curl_easy_setopt(cp, CURLOPT_CONNECTTIMEOUT_MS, YAR_G(connect_timeout));
#else
	curl_easy_setopt(cp, CURLOPT_CONNECTTIMEOUT, (zend_ulong)(YAR_G(connect_timeout) / 1000));
#endif
#if LIBCURL_VERSION_NUM > 0x071002
	curl_easy_setopt(cp, CURLOPT_TIMEOUT_MS, YAR_G(timeout));
#else
	curl_easy_setopt(cp, CURLOPT_TIMEOUT, (zend_ulong)(YAR_G(timeout) / 1000));
#endif

#if LIBCURL_VERSION_NUM >= 0x071100
	error = curl_easy_setopt(data->cp, CURLOPT_URL, ZSTR_VAL(address));
#else
	data->address = zend_string_copy(address);
	error = curl_easy_setopt(data->cp, CURLOPT_URL, ZSTR_VAL(data->address));
#endif

	if (error != CURLE_OK) {
		spprintf(msg, 0, "curl init failed '%s'", curl_easy_strerror(error));
		return 0;
	}

	return 1;
} /* }}} */

void php_yar_curl_close(yar_transport_interface_t* self) /* {{{ */ {
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;

	if (!data) {
		return;
	}

	if (data->cp) {
		if (!data->persistent) {
			curl_easy_cleanup(data->cp);
		} else {
			data->plink->in_use = 0;
		}
	}

#if LIBCURL_VERSION_NUM < 0x071100
	if (data->address) {
		zend_string_release((data->address));
	}
#endif

	if (data->host) {
		php_url_free(data->host);
	}

	smart_str_free(&data->buf);
	smart_str_free(&data->postfield);
	curl_slist_free_all(data->headers);

	efree(data);
	efree(self);

	return;
}
/* }}} */

static void php_yar_curl_prepare(yar_transport_interface_t* self) /* {{{ */ {
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;
	curl_easy_setopt(data->cp, CURLOPT_POSTFIELDS, ZSTR_VAL(data->postfield.s));
	curl_easy_setopt(data->cp, CURLOPT_POSTFIELDSIZE, ZSTR_LEN(data->postfield.s));

} /* }}} */

yar_response_t *php_yar_curl_exec(yar_transport_interface_t* self, yar_request_t *request) /* {{{ */ {
	char *msg;
	unsigned len;
	CURLcode ret;
	yar_response_t *response;
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;

	php_yar_curl_prepare(self);

	if (IS_ARRAY == Z_TYPE(request->options)) {
		zval *pzval;
		if ((pzval = zend_hash_index_find(Z_ARRVAL(request->options), YAR_OPT_TIMEOUT))) {
			convert_to_long_ex(pzval);
			self->setopt(self, YAR_OPT_TIMEOUT, (long *)&Z_LVAL_P(pzval), NULL);
		}
		if ((pzval = zend_hash_index_find(Z_ARRVAL(request->options), YAR_OPT_CONNECT_TIMEOUT))) {
			convert_to_long_ex(pzval);
			self->setopt(self, YAR_OPT_CONNECT_TIMEOUT, (long *)&Z_LVAL_P(pzval), NULL);
		}
	}

	response = php_yar_response_instance();

	ret = curl_easy_perform(data->cp);
	if (ret != CURLE_OK) {
		len = spprintf(&msg, 0, "curl exec failed '%s'", curl_easy_strerror(ret));
		php_yar_response_set_error(response, YAR_ERR_TRANSPORT, msg, len);
		efree(msg);
		return response;
	} else {
		long http_code;

		if(curl_easy_getinfo(data->cp, CURLINFO_RESPONSE_CODE, &http_code) == CURLE_OK 
				&& http_code != 200) {
			len = spprintf(&msg, 0, "server responsed non-200 code '%ld'", http_code);
			php_yar_response_set_error(response, YAR_ERR_TRANSPORT, msg, len);
			efree(msg);
			return response;
		}
	}

	if (data->buf.s) {
		zval *retval, ret;
		yar_header_t *header;
		char *payload;
		size_t payload_len;

		smart_str_0(&data->buf);

		payload = ZSTR_VAL(data->buf.s);
		payload_len = ZSTR_LEN(data->buf.s);

		if (!(header = php_yar_protocol_parse(payload))) {
			php_yar_error(response, YAR_ERR_PROTOCOL, "malformed response header '%.32s'", payload);
			return response;
		}

		/* skip over the leading header */
		payload += sizeof(yar_header_t);
		payload_len -= sizeof(yar_header_t);

		if (!(retval = php_yar_packager_unpack(payload, payload_len, &msg, &ret))) {
			php_yar_response_set_error(response, YAR_ERR_PACKAGER, msg, strlen(msg));
			efree(msg);
			return response;
		}

		php_yar_response_map_retval(response, retval);

		DEBUG_C(ZEND_ULONG_FMT": server response content packaged by '%.*s', len '%ld', content '%.32s'",
				response->id, 7, payload, header->body_len, payload + 8);

		zval_ptr_dtor(retval);
	} else {
		php_yar_response_set_error(response, YAR_ERR_EMPTY_RESPONSE, ZEND_STRL("empty response"));
	}	

	return response;
} /* }}} */

int php_yar_curl_send(yar_transport_interface_t* self, yar_request_t *request, char **msg) /* {{{ */ {
	yar_header_t header = {0};
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;
	zend_string *payload;

	if (!(payload = php_yar_request_pack(request, msg))) {
		return 0;
	}

	DEBUG_C(ZEND_ULONG_FMT": pack request by '%.*s', result len '%ld', content: '%.32s'", 
			request->id, 7, ZSTR_VAL(payload), ZSTR_LEN(payload), ZSTR_VAL(payload) + 8);

#if PHP_VERSION_ID < 70300
	php_yar_protocol_render(&header, request->id, data->host->user, data->host->pass, ZSTR_LEN(payload), 0);
#else
	php_yar_protocol_render(&header, request->id, data->host->user? ZSTR_VAL(data->host->user) : NULL, data->host->pass? ZSTR_VAL(data->host->pass) : NULL, ZSTR_LEN(payload), 0);
#endif

	smart_str_appendl(&data->postfield, (char *)&header, sizeof(yar_header_t));
	smart_str_appendl(&data->postfield, ZSTR_VAL(payload), ZSTR_LEN(payload));
	zend_string_release(payload);

	return 1;
} /* }}} */

int php_yar_curl_set_calldata(yar_transport_interface_t* self, yar_call_data_t *entry) /* {{{ */ {
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;
	data->calldata = entry;
	return 1;
} /* }}} */

int php_yar_curl_setopt(yar_transport_interface_t* self, long type, void *value, void *addtional) /* {{{ */ {
	yar_curl_data_t *data = (yar_curl_data_t *)self->data;
	CURL *cp = data->cp;
	
    switch (type) {
		case YAR_OPT_TIMEOUT:
#if LIBCURL_VERSION_NUM > 0x071002
			curl_easy_setopt(cp, CURLOPT_TIMEOUT_MS, *(long *) value);
#else
			curl_easy_setopt(cp, CURLOPT_TIMEOUT, ((zend_ulong)(*(long *)value) / 1000));
#endif
        break;
        case YAR_OPT_CONNECT_TIMEOUT:
#if LIBCURL_VERSION_NUM > 0x071002
			curl_easy_setopt(cp, CURLOPT_CONNECTTIMEOUT_MS, *(long *) value);
#else
			curl_easy_setopt(cp, CURLOPT_CONNECTTIMEOUT, ((zend_ulong)(*(long *)value) / 1000));
#endif
		break;
		default:
		    return 0;
	}
	return 1;
} /* }}} */

yar_transport_interface_t *php_yar_curl_init() /* {{{ */ {
	/* char content_type[512]; */
	yar_curl_data_t *data;
	yar_transport_interface_t *self;

	self = ecalloc(1, sizeof(yar_transport_interface_t));
	self->data = data = ecalloc(1, sizeof(yar_curl_data_t));

	/* snprintf(content_type, sizeof(content_type), "Content-Type: %s", YAR_G(content_type)); */
	data->headers = curl_slist_append(data->headers, "User-Agent: PHP Yar RPC-" PHP_YAR_VERSION);
	data->headers = curl_slist_append(data->headers, "Expect:");

	self->open   	= php_yar_curl_open;
	self->send   	= php_yar_curl_send;
	self->exec   	= php_yar_curl_exec;
	self->setopt	= php_yar_curl_setopt;
	self->calldata 	= php_yar_curl_set_calldata;
	self->close  	= php_yar_curl_close;


	smart_str_alloc((&data->buf), YAR_PACKAGER_BUFFER_SIZE /* 1M */, 0);
	smart_str_alloc((&data->postfield), YAR_PACKAGER_BUFFER_SIZE /* 1M */, 0);


	return  self;
} /* }}} */

void php_yar_curl_destroy(yar_transport_interface_t *self) /* {{{ */ {
} /* }}} */

int php_yar_curl_multi_add_handle(yar_transport_multi_interface_t *self, yar_transport_interface_t *handle) /* {{{ */ {
	yar_curl_multi_data_t *multi = (yar_curl_multi_data_t *)self->data;
	yar_curl_data_t *data = (yar_curl_data_t *)handle->data;

	php_yar_curl_prepare(handle);

	curl_multi_add_handle(multi->cm, data->cp);

	if (multi->chs) {
		data->next = multi->chs;
		multi->chs = handle;
	} else {
		multi->chs = handle;
	}

	return 1;
} /* }}} */

static int php_yar_curl_multi_parse_response(yar_curl_multi_data_t *multi, yar_concurrent_client_callback *f) /* {{{ */ {
	int msg_in_sequence;
	CURLMsg *msg;

	do {
		msg = curl_multi_info_read(multi->cm, &msg_in_sequence);
		if (msg && msg->msg == CURLMSG_DONE) {
			unsigned found = 0;
			yar_transport_interface_t *handle = multi->chs, *q = NULL;

			while (handle) {
				if (msg->easy_handle == ((yar_curl_data_t*)handle->data)->cp) {
					if (q) {
						((yar_curl_data_t *)q->data)->next = ((yar_curl_data_t*)handle->data)->next;
					} else {
						multi->chs = ((yar_curl_data_t*)handle->data)->next;
					}
					found = 1;
					break;
				}
				q = handle;
				handle = ((yar_curl_data_t*)handle->data)->next;
			}

			if (found) {
				long http_code = 200;
				yar_response_t *response;
				yar_curl_data_t *data = (yar_curl_data_t *)handle->data;

				response = php_yar_response_instance();

				if (msg->data.result == CURLE_OK) {
					curl_multi_remove_handle(multi->cm, data->cp);

					if(curl_easy_getinfo(data->cp, CURLINFO_RESPONSE_CODE, &http_code) == CURLE_OK && http_code != 200) {
						char buf[128];
						unsigned len = snprintf(buf, sizeof(buf), "server responsed non-200 code '%ld'", http_code);

						php_yar_response_set_error(response, YAR_ERR_TRANSPORT, buf, len);

						if (!f(data->calldata, YAR_ERR_TRANSPORT, response)) {
							/* if f return zero, means user call exit/die explicitly */
							handle->close(handle);
							php_yar_response_destroy(response);
							return -1;
						}
						if (EG(exception)) {
							/* uncaught exception */
							handle->close(handle);
							php_yar_response_destroy(response);
							return 0;
						}
						handle->close(handle);
						php_yar_response_destroy(response);
						continue;
					} else {
						if (data->buf.s) {
							char *msg = NULL;
							zval *retval, ret;
							yar_header_t *header;
							char *payload;
							size_t payload_len;

							smart_str_0(&data->buf);

							payload = ZSTR_VAL(data->buf.s);
							payload_len = ZSTR_LEN(data->buf.s);

							if (!(header = php_yar_protocol_parse(payload))) {
								php_yar_error(response, YAR_ERR_PROTOCOL, "malformed response header '%.32s'", payload);
							} else {
								/* skip over the leading header */
								payload += sizeof(yar_header_t);
								payload_len -= sizeof(yar_header_t);
								if (!(retval = php_yar_packager_unpack(payload, payload_len, &msg, &ret))) {
									php_yar_response_set_error(response, YAR_ERR_PACKAGER, msg, strlen(msg));
								} else {
									php_yar_response_map_retval(response, retval);
									DEBUG_C(ZEND_ULONG_FMT": server response content packaged by '%.*s', len '%ld', content '%.32s'", response->id, 7, payload, header->body_len, payload + 8);
									zval_ptr_dtor(retval);
								}
								if (msg) {
									efree(msg);
								}
							}
						} else {
							php_yar_response_set_error(response, YAR_ERR_EMPTY_RESPONSE, ZEND_STRL("empty response"));
						}

						if (!f(data->calldata, response->status, response)) {
							handle->close(handle);
							php_yar_response_destroy(response);
							return -1;
						}
						if (EG(exception)) {
							handle->close(handle);
							php_yar_response_destroy(response);
							return 0;
						}
					}
				} else {
					char *err = (char *)curl_easy_strerror(msg->data.result);
					php_yar_response_set_error(response, YAR_ERR_TRANSPORT, err, strlen(err));
					if (!f(data->calldata, YAR_ERR_TRANSPORT, response)) {
						handle->close(handle);
						php_yar_response_destroy(response);
						return -1;
					}
					if (EG(exception)) {
						handle->close(handle);
						php_yar_response_destroy(response);
						return 0;
					}
				}
				handle->close(handle);
				php_yar_response_destroy(response);
			} else {
				php_error_docref(NULL, E_WARNING, "unexpected transport info missed");
			}
		}
	} while (msg_in_sequence);

	return 1;
}
/* }}} */

int php_yar_curl_multi_exec(yar_transport_multi_interface_t *self, yar_concurrent_client_callback *f) /* {{{ */ {
	int running_count, rest_count;
	yar_curl_multi_data_t *multi = (yar_curl_multi_data_t *)self->data;

#ifdef ENABLE_EPOLL
	struct epoll_event events[8];
	yar_curl_multi_gdata g;

	g.epfd = epoll_create(YAR_EPOLL_MAX_SIZE);
	if (g.epfd == -1) {
		php_error_docref(NULL, E_WARNING, "epoll_create failed '%s'", strerror(errno));
		return 0;
	}

	if (!php_yar_start_timer(&g)) {
		close(g.epfd);
		return 0;
	}

	g.cm = multi->cm;
	/* setup the generic multi interface options we want */
	curl_multi_setopt(g.cm, CURLMOPT_SOCKETFUNCTION, php_yar_sock_cb);
	curl_multi_setopt(g.cm, CURLMOPT_SOCKETDATA, &g);
	curl_multi_setopt(g.cm, CURLMOPT_TIMERFUNCTION, php_yar_timer_cb);
	curl_multi_setopt(g.cm, CURLMOPT_TIMERDATA, &g);

	/* let's kick off everything */
	curl_multi_socket_action(g.cm, CURL_SOCKET_TIMEOUT, 0, &running_count);
	if (!f(NULL, YAR_ERR_OKEY, NULL)) {
		close(g.epfd);
		close(g.tfd);
		goto bailout;
	}
	if (EG(exception)) {
		close(g.epfd);
		close(g.tfd);
		goto onerror;
	}

	if (running_count) {
		rest_count = running_count;
		do {
			int idx;
			int ret = epoll_wait(g.epfd, events, sizeof(events)/sizeof(struct epoll_event), YAR_G(timeout));
			if (ret == -1) {
				if (errno == EINTR) {
					continue;
				} else {
					php_error_docref(NULL, E_WARNING, "epoll_wait error '%s'", strerror(errno));
					close(g.epfd);
					close(g.tfd);
					goto onerror;
				}
			}

			for (idx = 0; idx < ret; ++idx) {
				if (events[idx].data.fd == g.tfd) {
					size_t count;
					count = read(g.tfd, &count, sizeof(size_t));
					curl_multi_socket_action(g.cm, CURL_SOCKET_TIMEOUT, 0, &running_count);
				} else {
					if (events[idx].events & EPOLLOUT) {
						curl_multi_socket_action(g.cm, events[idx].data.fd, CURL_CSELECT_OUT, &running_count);
					} 
					if (events[idx].events & EPOLLIN) {
						curl_multi_socket_action(g.cm, events[idx].data.fd, CURL_CSELECT_IN, &running_count);
					}
				}
			}

			if (rest_count > running_count) {
				ret  = php_yar_curl_multi_parse_response(multi, f);
				if (ret == -1) {
					close(g.epfd);
					close(g.tfd);
					goto bailout;
				} else if (ret == 0) {
					close(g.epfd);
					close(g.tfd);
					goto onerror;
				}
				rest_count = running_count;
			}
		} while (running_count);
	} else {
		int ret = php_yar_curl_multi_parse_response(multi, f);
		if (ret == -1) {
			close(g.epfd);
			close(g.tfd);
			goto bailout;
		} else if (ret == 0) {
			close(g.epfd);
			close(g.tfd);
			goto onerror;
		}

	}
	close(g.tfd);
	close(g.epfd);

#else
	while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multi->cm, &running_count));

	if (!f(NULL, YAR_ERR_OKEY, NULL)) {
		goto bailout;
	}

	if (EG(exception)) {
		goto onerror;
	}

	if (running_count) {
		rest_count = running_count;
		do {
			int max_fd, return_code;
			struct timeval tv;
			fd_set readfds;
			fd_set writefds;
			fd_set exceptfds;

			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
			FD_ZERO(&exceptfds);

			curl_multi_fdset(multi->cm, &readfds, &writefds, &exceptfds, &max_fd);
			if (max_fd == -1) {
				long timeout;
#if LIBCURL_VERSION_NUM >= 0x070f04
				/* Available in 7.15.4 */
				curl_multi_timeout(multi->cm, &timeout);
				if (timeout < 0) {
					timeout = 50;
				}
				if (timeout) {
					tv.tv_sec = timeout / 1000;
					tv.tv_usec = (timeout % 1000) * 1000;
					select(1, &readfds, &writefds, &exceptfds, &tv);
				}
#else
				tv.tv_sec = 0;
				tv.tv_usec = 5000;
				select(1, &readfds, &writefds, &exceptfds, &tv);
#endif
				while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multi->cm, &running_count));
				goto process;
			}

			/* maybe we should use curl_multi_timeout like:
			 * curl_multi_timeout(curlm, (long *)&curl_timeout);
			 * if (curl_timeout == 0) {
			 * 	  continue;
			 * } else if (curl_timeout == -1) {
			 *	  tv.tv_sec = (zend_ulong)(YAR_G(timeout) / 1000);
			 *    tv.tv_usec = (zend_ulong)((YAR_G(timeout) % 1000)? (YAR_G(timeout) & 1000) * 1000 : 0);
			 * } else {
			 *    tv.tv_sec  =  curl_timeout / 1000;
			 * 	  tv.tv_usec = (curl_timeout % 1000) * 1000;
			 * }
			 */
			tv.tv_sec = (zend_ulong)(YAR_G(timeout) / 1000);
			tv.tv_usec = (zend_ulong)((YAR_G(timeout) % 1000)? (YAR_G(timeout) % 1000) * 1000 : 0);

			return_code = select(max_fd + 1, &readfds, &writefds, &exceptfds, &tv);
			if (return_code > 0) {
				while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multi->cm, &running_count));
			} else if (-1 == return_code) {
				php_error_docref(NULL, E_WARNING, "select error '%s'", strerror(errno));
				goto onerror;
			} else {
				/* timeout */
				php_error_docref(NULL, E_WARNING, "select timeout %ldms reached", YAR_G(timeout));
				goto onerror;
			}
process:
			if (rest_count > running_count) {
				int ret = php_yar_curl_multi_parse_response(multi, f);
				if (ret == -1) {
					goto bailout;
				} else if (ret == 0) {
					goto onerror;
				}
				rest_count = running_count;
			}
		} while (running_count);
	} else {
		int ret = php_yar_curl_multi_parse_response(multi, f);
		if (ret == -1) {
			goto bailout;
		} else if (ret == 0) {
			goto onerror;
		}
	}
#endif

	return 1;
onerror:
	return 0;
bailout:
	self->close(self);
	zend_bailout();
	return 0;
} /* }}} */

void php_yar_curl_multi_close(yar_transport_multi_interface_t *self) /* {{{ */ {
    yar_curl_multi_data_t *multi = (yar_curl_multi_data_t *)self->data;

	if (multi->chs) {
		yar_transport_interface_t *p, *q;
		p = multi->chs;
		for( ; p;) {
			yar_curl_data_t *data = (yar_curl_data_t *)p->data;
			q = data->next;
			curl_multi_remove_handle(multi->cm, data->cp);
			p->close(p);
			p = q;
		}
	}
	curl_multi_cleanup(multi->cm);
	efree(multi);
	efree(self);
	return ;
} /* }}} */

yar_transport_multi_interface_t * php_yar_curl_multi_init() /* {{{ */ {
	yar_transport_multi_interface_t *multi = emalloc(sizeof(yar_transport_multi_interface_t));
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
const yar_transport_t yar_transport_curl = {
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
