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
#include "hermes_packager.h"

struct _hermes_packagers_list {
	unsigned int size;
	unsigned int num;
	hermes_packager_t **packagers;
} hermes_packagers_list;

PHP_HERMES_API hermes_packager_t * php_hermes_packager_get(char *name, int nlen TSRMLS_DC) /* {{{ */ {
    int i = 0;
	for (;i<hermes_packagers_list.num;i++) {
		if (strncasecmp(hermes_packagers_list.packagers[i]->name, name, nlen) == 0) {
			return hermes_packagers_list.packagers[i];
		}
	}

	return NULL;
} /* }}} */

PHP_HERMES_API int php_hermes_packager_register(hermes_packager_t *packager TSRMLS_DC) /* {{{ */ {

	if (!hermes_packagers_list.size) {
	   hermes_packagers_list.size = 5;
	   hermes_packagers_list.packagers = (hermes_packager_t **)malloc(sizeof(hermes_packager_t *) * hermes_packagers_list.size);
	} else if (hermes_packagers_list.num == hermes_packagers_list.size) {
	   hermes_packagers_list.size += 5;
	   hermes_packagers_list.packagers = (hermes_packager_t **)realloc(hermes_packagers_list.packagers, sizeof(hermes_packager_t *) * hermes_packagers_list.size);
	}
	hermes_packagers_list.packagers[hermes_packagers_list.num] = packager;

	return hermes_packagers_list.num++;
} /* }}} */

size_t php_hermes_packager_pack(zval *pzval, char **payload, char **msg TSRMLS_DC) /* {{{ */ {
	smart_str buf = {0};
	char header[8];
	size_t newlen;
	hermes_packager_t *packager = HERMES_G(packager);
	memcpy(header, packager->name, 8);
	smart_str_alloc(&buf, HERMES_PACKAGER_BUFFER_SIZE /* 1M */, 0);
	smart_str_appendl(&buf, header, 8);
    packager->pack(packager, pzval, &buf, msg TSRMLS_CC); 

	if (buf.c) {
		*payload = buf.c;
		return buf.len;
	}

	smart_str_free(&buf);

	return 0;
} /* }}} */

zval * php_hermes_packager_unpack(char *content, size_t len, char **msg TSRMLS_DC) /* {{{ */ {
    char *pack_info = content; /* 4 bytes, last byte is version */
	hermes_packager_t *packager;
	content = content + 8;
    len -= 8;
	*(pack_info + 7) = '\0';
	packager = php_hermes_packager_get(pack_info, strlen(pack_info) TSRMLS_CC);

	if (!packager) {
		spprintf(msg, 0, "unsupported packager '%s'", pack_info);
		return NULL;
	}

	return packager->unpack(packager, content, len, msg TSRMLS_CC);

} /* }}} */

HERMES_STARTUP_FUNCTION(packager) /* {{{ */ {
#ifdef ENABLE_MSGPACK
  php_hermes_packager_register(&hermes_packager_msgpack TSRMLS_CC);
#endif
  php_hermes_packager_register(&hermes_packager_php TSRMLS_CC);
  php_hermes_packager_register(&hermes_packager_json TSRMLS_CC);

  return SUCCESS;
} /* }}} */

HERMES_ACTIVATE_FUNCTION(packager) /* {{{ */ {
	hermes_packager_t *packager = php_hermes_packager_get(HERMES_G(default_packager), strlen(HERMES_G(default_packager)) TSRMLS_CC);

    if (packager) {
		HERMES_G(packager) = packager;
		return SUCCESS;
	}

	HERMES_G(packager) = &hermes_packager_php;
	php_error(E_WARNING, "unable to find package '%s', use php instead", HERMES_G(default_packager));

	return SUCCESS;
} /* }}} */

HERMES_SHUTDOWN_FUNCTION(packager) /* {{{ */ {
	if (hermes_packagers_list.size) {
		free(hermes_packagers_list.packagers);
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
