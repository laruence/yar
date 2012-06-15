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

#ifndef PHP_HERMES_PROTOCOL_H
#define PHP_HERMES_PROTOCOL_H

typedef struct _hermes_protocol {
	const char *name;
	int (*request) (struct _hermes_protocol *self, struct _hermes_request *request, struct _hermes_response *response TSRMLS_DC);
	int (*async) (zval *callstack TSRMLS_DC);
} hermes_protocol_t;

PHP_HERMES_API int php_hermes_protocol_register(hermes_protocol_t *protocol TSRMLS_DC);
PHP_HERMES_API hermes_protocol_t * php_hermes_protocol_get(char *name, int nlen TSRMLS_DC);

HERMES_STARTUP_FUNCTION(protocol);

#endif	/* PHP_HERMES_PROTOCOL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
