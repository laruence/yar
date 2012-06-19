/*
  +----------------------------------------------------------------------+
  | Yar - Light, concurrent RPC framework                                |
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

#ifndef PHP_YAR_RESPONSE_H
#define PHP_YAR_RESPONSE_H

#define YAR_RESPONSE_REPLACE 0
#define YAR_RESPONSE_APPEND  1
#define YAR_RESPONSE_PREPEND 2

typedef struct _yar_response {
	long id;
	int  status;
	char *out;
	size_t out_len;
	zval *err;
	zval *retval;
} yar_response_t;

yar_response_t * php_yar_response_instance(TSRMLS_D);
int php_yar_response_bind_request(yar_response_t *response, struct _yar_request *request TSRMLS_DC);
void php_yar_response_alter_body(yar_response_t *response, char *body, uint len, int method TSRMLS_DC); 
void php_yar_response_set_error(yar_response_t *response, int type, char *message, uint len TSRMLS_DC); 
void php_yar_response_set_exception(yar_response_t *response, zval *ex TSRMLS_DC);
void php_yar_response_set_retval(yar_response_t *response, zval *retval TSRMLS_DC);
void php_yar_response_dtor(yar_response_t *response TSRMLS_DC);

#endif	/* PHP_YAR_RESPONSE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
