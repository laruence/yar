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

#include "php.h"
#include "php_yar.h"
#include "yar_packager.h"

struct _yar_packagers_list {
	unsigned int size;
	unsigned int num;
	const yar_packager_t **packagers;
} yar_packagers_list;

PHP_YAR_API const yar_packager_t * php_yar_packager_get(char *name, int nlen) /* {{{ */ {
    int i = 0;
	for (;i<yar_packagers_list.num;i++) {
		if (strncasecmp(yar_packagers_list.packagers[i]->name, name, nlen) == 0) {
			return yar_packagers_list.packagers[i];
		}
	}

	return NULL;
} /* }}} */

PHP_YAR_API int php_yar_packager_register(const yar_packager_t *packager) /* {{{ */ {

	if (!yar_packagers_list.size) {
	   yar_packagers_list.size = 5;
	   yar_packagers_list.packagers = (const yar_packager_t **)malloc(sizeof(yar_packager_t *) * yar_packagers_list.size);
	} else if (yar_packagers_list.num == yar_packagers_list.size) {
	   yar_packagers_list.size += 5;
	   yar_packagers_list.packagers = (const yar_packager_t **)realloc(yar_packagers_list.packagers, sizeof(yar_packager_t *) * yar_packagers_list.size);
	}
	yar_packagers_list.packagers[yar_packagers_list.num] = packager;

	return yar_packagers_list.num++;
} /* }}} */

zend_string *php_yar_packager_pack(char *packager_name, zval *pzval, char **msg) /* {{{ */ {
	char header[8];
	smart_str buf = {0};
	const yar_packager_t *packager = packager_name ?
		php_yar_packager_get(packager_name, strlen(packager_name)) : YAR_G(packager);

	if (!packager) {
		php_error_docref(NULL, E_ERROR, "unsupported packager %s", packager_name);
		return 0;
	}
	memcpy(header, packager->name, 8);
	smart_str_alloc(&buf, YAR_PACKAGER_BUFFER_SIZE /* 1M */, 0);
	smart_str_appendl(&buf, header, 8);
    packager->pack(packager, pzval, &buf, msg); 

	if (buf.s) {
		smart_str_0(&buf);
		return buf.s;
	}

	smart_str_free(&buf);

	return NULL;
} /* }}} */

zval * php_yar_packager_unpack(char *content, size_t len, char **msg, zval *rret) /* {{{ */ {
    char *pack_info = content; /* 4 bytes, last byte is version */
	const yar_packager_t *packager;

	content = content + 8;
    len -= 8;
	*(pack_info + 7) = '\0';
	packager = php_yar_packager_get(pack_info, 7);

	if (!packager) {
		spprintf(msg, 0, "unsupported packager '%s'", pack_info);
		return NULL;
	}

	return packager->unpack(packager, content, len, msg, rret);

} /* }}} */

YAR_STARTUP_FUNCTION(packager) /* {{{ */ {
#ifdef ENABLE_MSGPACK
  php_yar_packager_register(&yar_packager_msgpack);
#endif
  php_yar_packager_register(&yar_packager_php);
  php_yar_packager_register(&yar_packager_json);

  REGISTER_STRINGL_CONSTANT("YAR_PACKAGER_PHP", YAR_PACKAGER_PHP, sizeof(YAR_PACKAGER_PHP)-1, CONST_CS|CONST_PERSISTENT);
  REGISTER_STRINGL_CONSTANT("YAR_PACKAGER_JSON", YAR_PACKAGER_JSON, sizeof(YAR_PACKAGER_JSON)-1, CONST_CS|CONST_PERSISTENT);
#ifdef ENABLE_MSGPACK
  REGISTER_STRINGL_CONSTANT("YAR_PACKAGER_MSGPACK", YAR_PACKAGER_MSGPACK, sizeof(YAR_PACKAGER_MSGPACK)-1, CONST_CS|CONST_PERSISTENT);
#endif

  return SUCCESS;
} /* }}} */

YAR_ACTIVATE_FUNCTION(packager) /* {{{ */ {
	const yar_packager_t *packager = php_yar_packager_get(YAR_G(default_packager), strlen(YAR_G(default_packager)));

    if (packager) {
		YAR_G(packager) = packager;
		return SUCCESS;
	}

	YAR_G(packager) = &yar_packager_php;
	php_error(E_WARNING, "unable to find package '%s', use php instead", YAR_G(default_packager));

	return SUCCESS;
} /* }}} */

YAR_SHUTDOWN_FUNCTION(packager) /* {{{ */ {
	if (yar_packagers_list.size) {
		free(yar_packagers_list.packagers);
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
