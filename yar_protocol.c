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
#include "yar_protocol.h"
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

void php_yar_protocol_render(yar_header_t *header, uint32_t id, char *provider, char *token, uint32_t body_len, uint32_t reserved) /* {{{ */ {
	header->magic_num = htonl(YAR_PROTOCOL_MAGIC_NUM);
	header->id = htonl(id);
	header->body_len = htonl(body_len);
	header->reserved = htonl(reserved);
	if (provider) {
		memcpy(header->provider, provider, MIN(strlen(provider), 32));
	}
	if (token) {
		memcpy(header->token, token, MIN(strlen(token), 32));
	}
} /* }}} */

yar_header_t * php_yar_protocol_parse(char *payload) /* {{{ */ {
	yar_header_t *header = (yar_header_t *)payload;

	header->magic_num = ntohl(header->magic_num);

	if (header->magic_num != YAR_PROTOCOL_MAGIC_NUM) {
		header->magic_num = htonl(header->magic_num);
		return NULL;
	}

	header->id = ntohl(header->id);
	header->body_len = ntohl(header->body_len);
	header->reserved = ntohl(header->reserved);

	return header;
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
