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

struct _hermes_transports_list {
	unsigned int size;
	unsigned int num;
	hermes_transport_t **transports;
} hermes_transports_list;

extern hermes_transport_t hermes_transport_curl;

PHP_HERMES_API hermes_transport_t * php_hermes_transport_get(char *name, int nlen TSRMLS_DC) /* {{{ */ {
    int i = 0;
	for (;i<hermes_transports_list.num;i++) {
		if (strncmp(hermes_transports_list.transports[i]->name, name, nlen) == 0) {
			return hermes_transports_list.transports[i];
		}
	}

	return NULL;
} /* }}} */

PHP_HERMES_API int php_hermes_transport_register(hermes_transport_t *transport TSRMLS_DC) /* {{{ */ {

	if (!hermes_transports_list.size) {
	   hermes_transports_list.size = 5;
	   hermes_transports_list.transports = (hermes_transport_t **)malloc(sizeof(hermes_transport_t *) * hermes_transports_list.size);
	} else if (hermes_transports_list.num == hermes_transports_list.size) {
	   hermes_transports_list.size += 5;
	   hermes_transports_list.transports = (hermes_transport_t **)realloc(hermes_transports_list.transports, sizeof(hermes_transport_t *) * hermes_transports_list.size);
	}
	hermes_transports_list.transports[hermes_transports_list.num] = transport;

	return hermes_transports_list.num++;
} /* }}} */

HERMES_STARTUP_FUNCTION(transport) /* {{{ */ {
  php_hermes_transport_register(&hermes_transport_curl TSRMLS_CC);

  return SUCCESS;
} /* }}} */

HERMES_ACTIVATE_FUNCTION(transport) /* {{{ */ {

	HERMES_G(transport) = &hermes_transport_curl;

	return SUCCESS;
} /* }}} */

HERMES_SHUTDOWN_FUNCTION(transport) /* {{{ */ {
	if (hermes_transports_list.size) {
		free(hermes_transports_list.transports);
	}
	return SUCCESS;
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
