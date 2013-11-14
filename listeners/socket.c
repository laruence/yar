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
  |                                                                      |
  | Fork:    Qinghui Zeng  <zengohm@gmail.com>                           |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_network.h"

#ifndef PHP_WIN32
#define php_select(m, r, w, e, t)   select(m, r, w, e, t)
#else
#include "win32/select.h"
#endif


#include "php_yar.h"
#include "yar_protocol.h"
#include "yar_response.h"
#include "yar_request.h"
#include "yar_listener.h"
#include "yar_packager.h"
#include "yar_exception.h"
#include "ext/standard/php_var.h" /* for serialize */
#include "ext/standard/php_smart_str.h" /* for smart string */

#ifdef ENABLE_EPOLL
#include <sys/epoll.h>
#define YAR_EPOLL_MAX_SIZE 128
#endif

#define SEND_BUF_SIZE 1280
#define RECV_BUF_SIZE 1280

typedef struct _yar_listener_data_client_t{
    php_stream *stream;
    int is_live;
    pthread_t pid;
} yar_listener_data_client_t;

typedef struct _yar_listener_date_t{
    unsigned int size;
    unsigned int num;
    php_stream *server;
    yar_listener_data_client_t **clients;
    int (*register_client)(struct _yar_listener_date_t *data, struct _yar_listener_data_client_t *client TSRMLS_DC);
} yar_listener_data_t;

int php_yar_listener_data_client_register(yar_listener_data_t *data, yar_listener_data_client_t *client TSRMLS_DC) /* {{{ */ {
    if (!data->size) {
        data->size = 5;
        data->clients = (yar_listener_data_client_t **)malloc(sizeof(yar_listener_data_client_t *) * data->size);
     } else if (data->num == data->size) {
        data->size += 5;
        data->clients = (yar_listener_data_client_t **)realloc(data->clients, sizeof(yar_listener_data_client_t *) * data->size);
     }
     data->clients[data->num] = client;

     return data->num++;
}
/* }}} */



void php_yar_listener_socket_response(php_stream *client, yar_request_t *request, yar_response_t *response TSRMLS_DC) /* {{{ */ {
    zval ret;
    char *payload, *err_msg;
    size_t payload_len;
    yar_header_t header = {0};

    INIT_ZVAL(ret);
    array_init(&ret);

    add_assoc_long_ex(&ret, ZEND_STRS("i"), response->id);
    add_assoc_long_ex(&ret, ZEND_STRS("s"), response->status);
    if (response->out) {
            add_assoc_string_ex(&ret, ZEND_STRS("o"), response->out, 1);
    }
    if (response->retval) {
            Z_ADDREF_P(response->retval);
            add_assoc_zval_ex(&ret, ZEND_STRS("r"), response->retval);
    }
    if (response->err) {
            Z_ADDREF_P(response->err);
            add_assoc_zval_ex(&ret, ZEND_STRS("e"), response->err);
    }

    if (!(payload_len = php_yar_packager_pack(NULL, &ret, &payload, &err_msg TSRMLS_CC))) {
            zval_dtor(&ret);
            php_yar_error(response, YAR_ERR_PACKAGER TSRMLS_CC, "%s", err_msg);
            efree(err_msg);
            return;
    }
    zval_dtor(&ret);

    php_yar_protocol_render(&header, request->id, "PHP Yar Server", NULL, payload_len, 0 TSRMLS_CC);

    DEBUG_S("%ld: server response: packager '%s', len '%ld', content '%.32s'",
                    request->id, payload, payload_len - 8, payload + 8);

    //php_yar_listener_curl_response_header(sizeof(yar_header_t) + payload_len, payload TSRMLS_CC);
    //PHPWRITE((char *)&header, sizeof(yar_header_t));
    if(client){
            php_stream_xport_sendto(client,(char *)&header, sizeof(yar_header_t), 0, NULL, 0 TSRMLS_CC);
    }
    if (payload_len) {
        if(client){
            php_stream_xport_sendto(client,payload, payload_len, 0, NULL, 0 TSRMLS_CC);            
        }
        //PHPWRITE(payload, payload_len);
        efree(payload);
        return;
    }

    return;
}
/* }}} */

int php_yar_listener_socket_listen(yar_listener_interface_t *self, char *address, uint len, zval *executor, char **err_msg  TSRMLS_DC) /* {{{ */ {
    yar_listener_data_t * data = (yar_listener_data_t*)self->data;
    php_stream *server = NULL;
    yar_listener_data_client_t *client_stream = NULL;
    char *errstr = NULL, *persistent_key = NULL;
    int err;
    
    self->executor = executor;
    
    server = php_stream_xport_create(address, len, ENFORCE_SAFE_MODE | REPORT_ERRORS,
            STREAM_XPORT_SERVER | STREAM_XPORT_BIND | STREAM_XPORT_LISTEN, persistent_key, NULL, NULL, &errstr, &err);

    if (persistent_key) {
        efree(persistent_key);
    }
    
    if(errstr){
        efree(errstr);
    }
    
    if (server == NULL) {
            spprintf(err_msg, 0, "Unable to listen to %s (%s)", address, strerror(errno));
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "unable to listen to %s (%s)", address, errstr == NULL ? "Unknown error" : errstr);
            efree(errstr);
            return 0;
    }

    //php_stream_set_option(server, PHP_STREAM_OPTION_BLOCKING, 0, NULL);
    
#if ZEND_DEBUG
    server->__exposed++;
#endif

    data->server = server;
    client_stream = (yar_listener_data_client_t *)emalloc(sizeof(yar_listener_data_client_t));
    while(1){
        php_stream_xport_accept(server,&(client_stream->stream),NULL,NULL,NULL,NULL,NULL,&errstr TSRMLS_CC);
        client_stream->is_live = 1;
        data->register_client(data, client_stream);
        //pthread_create(&(client_stream->pid),NULL,self->accept,NULL);
        //self->accept(self,(void *)client_stream);
        while(self->accept(self,(void *)client_stream)){
            ;
        }
        if(client_stream->stream){
            php_stream_close(client_stream->stream);
        }
    }
    efree(client_stream);   
    
    if(errstr){
        efree(errstr);
    }
    
    return 1;
}
/* }}} */

int php_yar_listener_socket_accept(yar_listener_interface_t *self, void* client TSRMLS_DC) /* {{{ */ {
    yar_response_t *response;
    yar_request_t *request;
    yar_listener_data_client_t* socket_client = (yar_listener_data_client_t *)client;

    response = php_yar_response_instance(TSRMLS_C);
    if(self->recv(self, client, &request)>0){
        self->exec(self, request, response);
        php_yar_listener_socket_response(socket_client->stream, request, response TSRMLS_CC);
        php_yar_request_destroy(request TSRMLS_CC);
        php_yar_response_destroy(response TSRMLS_CC);
        return 1;
    }else{
        php_yar_response_destroy(response TSRMLS_CC);
        return 0;
    }
    
}
/* }}} */

int php_yar_listener_socket_recv( yar_listener_interface_t *self, void* client,yar_request_t ** request TSRMLS_DC) /* {{{ */ {
    int recvd;
    yar_header_t *header;
    char read_buf[RECV_BUF_SIZE];
    char *payload, *err_msg = NULL;
    zval* temp;
    yar_listener_data_client_t* socket_client = (yar_listener_data_client_t *)client;
     


    if(!socket_client->stream){
        return -1;
    }
    
    
    if((recvd = php_stream_xport_recvfrom(socket_client->stream, read_buf, RECV_BUF_SIZE, 0, NULL, NULL, NULL, NULL TSRMLS_CC))<=0){
        return -1;
    }
    if (!(header = php_yar_protocol_parse(read_buf TSRMLS_CC))) {
        //php_yar_error(NULL, YAR_ERR_PROTOCOL TSRMLS_CC, "malformed request header '%.32s'", read_buf);
        return 0;
    }
    
    payload = emalloc(header->body_len);
    memcpy(payload, read_buf + sizeof(yar_header_t), recvd - sizeof(yar_header_t));
    if(header->body_len + sizeof(yar_header_t) > RECV_BUF_SIZE){
        recvd = php_stream_xport_recvfrom(socket_client->stream, payload + RECV_BUF_SIZE - sizeof(yar_header_t),
                header->body_len + sizeof(yar_header_t) - RECV_BUF_SIZE, 0, NULL, NULL, NULL, NULL TSRMLS_CC);
        //memcpy(payload + RECV_BUF_SIZE - sizeof(yar_header_t), read_buf, recvd);
    }
    temp = php_yar_packager_unpack(payload, header->body_len, &err_msg TSRMLS_CC);
    efree(payload);
    *request = php_yar_request_unpack(temp);
    zval_dtor(temp);
    if(err_msg){
        efree(err_msg);
    }
}
/* }}} */

int php_yar_listener_socket_exec( yar_listener_interface_t *self, yar_request_t *request, yar_response_t *response TSRMLS_DC) /* {{{ */ {
    char *msg, *err_msg, method[256];
    zend_class_entry *ce;
    zend_bool bailout = 0;
    zval output, func;
    
    if (!php_yar_request_valid(request, response, &err_msg TSRMLS_CC)) {
        php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "%s", err_msg);
        efree(err_msg);
        return 0;
    }
    
    ce = Z_OBJCE_P(self->executor);
    zend_str_tolower_copy(method, request->method, request->mlen);
    if (!zend_hash_exists(&ce->function_table, method, strlen(method) + 1)) {
        php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "call to undefined api %s::%s()", ce->name, request->method);
        return 0;
    }
    
    zend_try {
            uint count;
            zval ***func_params;
            zval *retval_ptr = NULL;
            HashTable *func_params_ht;

            INIT_ZVAL(output);
#if 0 
            if (zend_hash_exists(&ce->function_table, ZEND_STRS("__auth"))) {
                    zval *provider, *token;
                    MAKE_STD_ZVAL(provider);
                    MAKE_STD_ZVAL(token);
                    if (header->provider) {
                            ZVAL_STRING(provider, (char *)header->provider, 1);
                    } else {
                            ZVAL_NULL(provider);
                    }

                    if (header->token) {
                            ZVAL_STRING(token, (char *)header->token, 1);
                    } else {
                            ZVAL_NULL(token);
                    }

                    func_params = emalloc(sizeof(zval **) * 2);
                    func_params[0] = &provider;
                    func_params[1] = &token;

                    ZVAL_STRINGL(&func, "__auth", sizeof("__auth") - 1, 0);
                    if (call_user_function_ex(NULL, &obj, &func, &retval_ptr, 2, func_params, 0, NULL TSRMLS_CC) != SUCCESS) {
                            efree(func_params);
                            zval_ptr_dtor(&token);
                            zval_ptr_dtor(&provider);
                            php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "call to api %s::%s() failed", ce->name, Z_STRVAL(func));
                            goto response;
                    }

                    efree(func_params);
        func_params = NULL;
                    zval_ptr_dtor(&token);
                    zval_ptr_dtor(&provider);

                    if (retval_ptr) {
           if (Z_TYPE_P(retval_ptr) == IS_BOOL && !Z_BVAL_P(retval_ptr)) {
                               zval_ptr_dtor(&retval_ptr);
                               php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "%s::__auth() return false", ce->name);
                               goto response;
                       }
                       zval_ptr_dtor(&retval_ptr);
                       retval_ptr = NULL;
                    }
            }
#endif

            func_params_ht = Z_ARRVAL_P(request->parameters);
            count = zend_hash_num_elements(func_params_ht);

            if (count) {
                    uint i = 0;
                    func_params = emalloc(sizeof(zval **) * count);

                    for (zend_hash_internal_pointer_reset(func_params_ht);
                                    zend_hash_get_current_data(func_params_ht, (void **) &func_params[i++]) == SUCCESS;
                                    zend_hash_move_forward(func_params_ht)
                            );
            } else {
                    func_params = NULL;
            }

            ZVAL_STRINGL(&func, request->method, request->mlen, 0);
            if (call_user_function_ex(NULL, &(self->executor), &func, &retval_ptr, count, func_params, 0, NULL TSRMLS_CC) != SUCCESS) {
                    if (func_params) {
                            efree(func_params);
                    }
                php_yar_error(response, YAR_ERR_REQUEST TSRMLS_CC, "call to api %s::%s() failed", ce->name, request->method);
                    goto response;
            }

            if (func_params) {
                    efree(func_params);
            }

            if (retval_ptr) {
                    php_yar_response_set_retval(response, retval_ptr TSRMLS_CC);
            }
    } zend_catch {
            bailout = 1;
    } zend_end_try();
    
    if (EG(exception)) {
        php_yar_response_set_exception(response, EG(exception) TSRMLS_CC);
        EG(exception) = NULL;
    }

    response:
    #if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4))
            php_ob_get_buffer(&output TSRMLS_CC);
            php_end_ob_buffer(0, 0 TSRMLS_CC);
    #else
            if (php_output_get_contents(&output TSRMLS_CC) == FAILURE) {
                    php_output_end(TSRMLS_C);
                    php_yar_error(response, YAR_ERR_OUTPUT TSRMLS_CC, "unable to get ob content");
                    //goto response_no_output;
                    return 0;
            }

            php_output_discard(TSRMLS_C);
    #endif
    php_yar_response_alter_body(response, Z_STRVAL(output), Z_STRLEN(output), YAR_RESPONSE_REPLACE TSRMLS_CC);

    if (bailout) {
            zend_bailout();
    }
    return 0;
} 
/* }}} */

void php_yar_listener_socket_close( yar_listener_interface_t *self TSRMLS_DC) /* {{{ */ {
    yar_listener_data_t *data = (yar_listener_data_t *)self->data;
    //php_stream_xport_shutdown(data->server,STREAM_SHUT_RDWR);
    php_stream_close(data->server);
    return;
} 
/* }}} */

yar_listener_interface_t * php_yar_listener_socket_init(TSRMLS_D) /* {{{ */ {
    yar_listener_interface_t *self;
    yar_listener_data_t *data = (yar_listener_data_t *)emalloc(sizeof(yar_listener_data_t));
    data->register_client = php_yar_listener_data_client_register;
    data->num=0;
    data->size=0;
    
    self = ecalloc(1, sizeof(yar_listener_interface_t));
    self->data = (void *)data;
    
    self->executor = NULL;
    
    self->listen = php_yar_listener_socket_listen;
    self->accept = php_yar_listener_socket_accept;
    self->recv = php_yar_listener_socket_recv;
    self->exec = php_yar_listener_socket_exec;
    self->close = php_yar_listener_socket_close;
    
    return self;
} 
/* }}} */

void php_yar_listener_socket_destroy(yar_listener_interface_t *self TSRMLS_DC) /* {{{ */ {
    efree(self->data);
} 
/* }}} */

/* {{{ yar_listener_t yar_listener_socket
 */
yar_listener_t yar_listener_socket = {
	"socket",
	php_yar_listener_socket_init,
	php_yar_listener_socket_destroy,
	NULL
}; 
/* }}} */