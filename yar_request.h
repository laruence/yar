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

#ifndef PHP_YAR_REQUEST_H
#define PHP_YAR_REQUEST_H

typedef struct _yar_request {
	zend_ulong id;
	zend_string *method;
	zval parameters;
	/* following fileds don't going to packager */
	zval options;
} yar_request_t;

yar_request_t * php_yar_request_unpack(zval *body);
void php_yar_request_destroy(yar_request_t *request);
zend_string *php_yar_request_pack(yar_request_t *request, char **msg);
int php_yar_request_valid(yar_request_t *req, struct _yar_response *response, char **msg);
yar_request_t * php_yar_request_instance(zend_string *method, zval *params, zval *options);

#endif	/* PHP_YAR_REQUEST_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
