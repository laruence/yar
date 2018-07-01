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

#ifndef PHP_YAR_EXCEPTION_H
#define PHP_YAR_EXCEPTION_H

#define YAR_ERR_OKEY      		0x0
#define YAR_ERR_PACKAGER  		0x1
#define YAR_ERR_PROTOCOL  		0x2
#define YAR_ERR_REQUEST   		0x4
#define YAR_ERR_OUTPUT    		0x8
#define YAR_ERR_TRANSPORT 		0x10
#define YAR_ERR_FORBIDDEN 		0x20
#define YAR_ERR_EXCEPTION 		0x40
#define YAR_ERR_EMPTY_RESPONSE 	0x80

extern zend_class_entry *yar_server_exception_ce;
extern zend_class_entry *yar_server_request_exception_ce;
extern zend_class_entry *yar_server_protocol_exception_ce;
extern zend_class_entry *yar_server_packager_exception_ce;
extern zend_class_entry *yar_server_output_exception_ce;

extern zend_class_entry *yar_client_exception_ce;
extern zend_class_entry *yar_client_transport_exception_ce;
extern zend_class_entry *yar_client_packager_exception_ce;
extern zend_class_entry *yar_client_protocol_exception_ce;

extern void (*zend_orig_error_cb)(int, const char *, const uint, const char *, va_list);
 
void php_yar_error_ex(struct _yar_response *response, int type, const char *format, va_list args);
void php_yar_error(struct _yar_response *response, int type, const char *format, ...);

YAR_STARTUP_FUNCTION(exception);

#define DEBUG_S(fmt, ...) \
	do { \
		if (UNEXPECTED(YAR_G(debug))) { \
			 php_yar_debug_server(fmt, ##__VA_ARGS__); \
		} \
	} while (0);

#define DEBUG_C(fmt, ...) \
	do { \
		if (UNEXPECTED(YAR_G(debug))) { \
			 php_yar_debug_client(fmt, ##__VA_ARGS__); \
		} \
	} while (0);

static inline void php_yar_debug_server(const char *format, ...) {
	va_list args;
	char buf[1024];
	char *message;
	struct timeval tv;
	struct tm *t;

	gettimeofday(&tv, NULL);
	t = localtime(&tv.tv_sec);

	va_start(args, format);
	snprintf(buf, sizeof(buf), "[Debug Yar_Server %d:%d:%d.%ld]: %s", t->tm_hour, t->tm_min, t->tm_sec, tv.tv_usec, format);
	vspprintf(&message, 0, buf, args);
	php_error(E_WARNING, "%s", message);
	efree(message);
}

static inline void php_yar_debug_client(const char *format, ...) {
	va_list args;
	char buf[1024];
	char *message;
	struct timeval tv;
	struct tm *t;

	gettimeofday(&tv, NULL);
	t = localtime(&tv.tv_sec);

	va_start(args, format);
	snprintf(buf, sizeof(buf), "[Debug Yar_Client %d:%d:%d.%ld]: %s", t->tm_hour, t->tm_min, t->tm_sec, tv.tv_usec, format);
	vspprintf(&message, 0, buf, args);
	php_error(E_WARNING, "%s", message);
	efree(message);
}

YAR_STARTUP_FUNCTION(exception);

#endif	/* PHP_YAR_EXCEPTION_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
