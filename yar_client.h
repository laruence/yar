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

#ifndef PHP_YAR_CLIENT_H
#define PHP_YAR_CLIENT_H

#define YAR_CLIENT_PROTOCOL_HTTP  1
#define YAR_CLIENT_PROTOCOL_TCP   2
#define YAR_CLIENT_PROTOCOL_UDP   3
#define YAR_CLIENT_PROTOCOL_UNIX  4

#ifndef ZEND_FETCH_RESOURCE_NO_RETURN
#define ZEND_FETCH_RESOURCE_NO_RETURN(rsrc, rsrc_type, passed_id, default_id, resource_type_name, resource_type)    \
	    (rsrc = (rsrc_type) zend_fetch_resource(passed_id TSRMLS_CC, default_id, resource_type_name, NULL, 1, resource_type))
#endif

YAR_STARTUP_FUNCTION(client);
YAR_SHUTDOWN_FUNCTION(client);

#endif	/* PHP_YAR_CLIENT_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
