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
#include "yar_protocol.h"
#include <arpa/inet.h>

void php_yar_protocol_render(yar_header_t *header, uint id, char *provider, char *token, uint body_len, uint reserved TSRMLS_DC) /* {{{ */ {
	header->magic_num = htonl(YAR_PROTOCOL_MAGIC_NUM);
	header->id = htonl(id);
	header->body_len = htonl(body_len);
	header->reserved = htonl(reserved);
	if (provider) {
		memcpy(header->provider, provider, 16);
	}
	if (token) {
		memcpy(header->token, token, 16);
	}
	return;
} /* }}} */

yar_header_t * php_yar_protocol_parse(char **payload, size_t *payload_len, char **err_msg TSRMLS_DC) /* {{{ */ {
	yar_header_t *header = (yar_header_t *)*payload;

	header->magic_num = ntohl(header->magic_num);

	if (header->magic_num != YAR_PROTOCOL_MAGIC_NUM) {
		spprintf(err_msg, 0, "malformed protocol header, maybe not responsed/sent by a yar rpc server/client?");
		return NULL;
	}

	header->id = ntohl(header->id);
	header->body_len = ntohl(header->body_len);
	header->reserved = ntohl(header->reserved);
  
	*payload += sizeof(yar_header_t);
	*payload_len -= sizeof(yar_header_t);

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
