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

#define YAR_LISTENER_SOCKET_CLIENT_OPEN_MAX 128
#define YAR_LISTENER_SOCKET_CLIENT_LISTENQ 14

#ifndef INFTIM
#define YAR_LISTENER_SOCKET_INFTIM -1
#else
#define YAR_LISTENER_SOCKET_INFTIM INFTIM
#endif

typedef struct _yar_listener_data_client_t{
    yar_header_t header;
    char* read_buff;
    ssize_t header_offset;
    ssize_t buff_offset;
    int is_read_finished;
} yar_listener_data_client_t;

typedef struct _yar_listener_date_t{
    int listenfd;
    yar_listener_data_client_t clients_data[YAR_LISTENER_SOCKET_CLIENT_OPEN_MAX];
    struct pollfd clients_socket[YAR_LISTENER_SOCKET_CLIENT_OPEN_MAX];
} yar_listener_data_t;

static int yar_listener_socket_running = 0;

static void yar_listener_socket_sigint_handler(int sig) /* {{{ */
{
	yar_listener_socket_running = 0;
}
/* }}} */

void php_yar_listener_socket_response(int client, yar_request_t *request, yar_response_t *response TSRMLS_DC) /* {{{ */ {
    zval ret;
    char *payload, *err_msg;
    size_t payload_len, write_len;
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
    if(client){
        write_len = write(client,(char *)&header, sizeof(yar_header_t));
        if (write_len > 0 && payload_len) {
            write_len = write(client,payload, payload_len);
            efree(payload);
        }
    }
}
/* }}} */

void php_yar_listener_socket_clear_client_data(yar_listener_data_client_t* client) /* {{{ */
{
    client->buff_offset = 0;
    client->header_offset = 0;
    if(client->read_buff != NULL){
        efree(client->read_buff);
        client->read_buff = NULL;
    }
    client->is_read_finished = 0;
}
/* }}} */

int php_yar_listener_socket_handle(yar_listener_interface_t *self, char *address, uint len, zval *executor, char **err_msg  TSRMLS_DC) /* {{{ */ {
    yar_listener_data_t * data = (yar_listener_data_t*)self->data;
    int listenfd, connfd, i, maxi, sockfd, flag;
    int nready;
    ssize_t recv_byte, left_recv_byte;
    char buf[RECV_BUF_SIZE];
    socklen_t clilen;
    struct pollfd *client = data->clients_socket;
    struct sockaddr_in cliaddr, servaddr;
    char *host;
    char *p;
    int port;
    zval* temp;
    yar_request_t * request;
    yar_response_t *response;
    yar_listener_data_client_t* cursor_data;
    
    self->executor = executor;
    
    host = pestrdup(address+6,1);
    if(!host){
        php_error(E_ERROR,"Yar cannot listen to address %s",address);
        return -1;
    }
    p = strchr(host,':');
    if(p){
        *p++= '\0';
        port = strtol(p,&p,10);
    }
    if(port<=0){
        php_error(E_ERROR,"Yar cannot listen to address %s",address);
        return -1;
    }

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if(listenfd<=0){
        php_error(E_ERROR,"Yar cannot listen to address %s",address);
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(host);
    pefree(host,1);
    servaddr.sin_port = htons(port);
    flag = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if(flag!=0){
        php_error(E_ERROR,"Yar cannot bind to address %s",address);
        return -1;
    }
    flag = listen(listenfd, YAR_LISTENER_SOCKET_CLIENT_LISTENQ);
    if(flag!=0){
        php_error(E_ERROR,"Yar cannot listen to address %s",address);
        return -1;
    }
    client[0].fd = listenfd;
    client[0].events = POLLRDNORM;
    for(i=1;i < YAR_LISTENER_SOCKET_CLIENT_OPEN_MAX; i++){
        client[i].fd = -1;
        data->clients_data[i].read_buff = NULL;
    }

    maxi = 0;
    data->listenfd = listenfd;
    yar_listener_socket_running = 1;
    #if defined(HAVE_SIGNAL_H) && defined(SIGINT)
        signal(SIGINT, yar_listener_socket_sigint_handler);
    #endif
    while(yar_listener_socket_running){
        nready = poll(client, maxi+1, YAR_LISTENER_SOCKET_INFTIM);

        if(client[0].revents & POLLRDNORM) {
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (__SOCKADDR_ARG)&cliaddr, &clilen);

            for(i=1;i<YAR_LISTENER_SOCKET_CLIENT_OPEN_MAX;i++){
                if(client[i].fd<0){
                    client[i].fd = connfd;
                    data->clients_data[i].read_buff = NULL;
                    data->clients_data[i].is_read_finished = 0;
                    data->clients_data[i].header_offset = 0;
                    data->clients_data[i].buff_offset = 0;
                    break;
                }
            }

            if(i==YAR_LISTENER_SOCKET_CLIENT_OPEN_MAX){
                php_error(E_WARNING,"Yar recive too many clients");
                close(connfd);
                continue;
            }

            client[i].events = POLLRDNORM;

            if(i>maxi){
                maxi = i;
            }

            if(--nready <= 0){
                continue;
            }

        }

        for(i=1;i<=maxi;i++){
            if((sockfd=client[i].fd)<0){
                continue;
            }

            if(client[i].revents & (POLLRDNORM | POLLERR)){
                recv_byte=read(sockfd,buf,RECV_BUF_SIZE);
                if(recv_byte<0){
                    close(sockfd);
                    client[i].fd = -1;
                    php_yar_listener_socket_clear_client_data(&(data->clients_data[i]));
                }else if(recv_byte==0){
                    close(sockfd);
                    client[i].fd = -1;
                    php_yar_listener_socket_clear_client_data(&(data->clients_data[i]));
                } else {
                    cursor_data = &(data->clients_data[i]);
                    if(cursor_data->header_offset<sizeof(yar_header_t)){
                        if(recv_byte < (sizeof(yar_header_t) - cursor_data->header_offset)){
                            memcpy((char *)&cursor_data->header + cursor_data->header_offset, buf, recv_byte);
                            cursor_data->header_offset += recv_byte;
                        }else{
                            left_recv_byte = (sizeof(yar_header_t) - cursor_data->header_offset);
                            memcpy((char *)&cursor_data->header + cursor_data->header_offset, buf, left_recv_byte);
                            cursor_data->header_offset = sizeof(yar_header_t);
                            php_yar_protocol_parse((char *)&cursor_data->header TSRMLS_CC);
                            cursor_data->read_buff = (char *)emalloc(cursor_data->header.body_len);
                            memcpy(cursor_data->read_buff, buf + left_recv_byte, recv_byte - left_recv_byte);
                            cursor_data->buff_offset = recv_byte - left_recv_byte;
                            left_recv_byte = recv_byte;
                            if(cursor_data->buff_offset == cursor_data->header.body_len){
                                cursor_data->is_read_finished = 1;
                            }
                        }
                    }else{
                        if(recv_byte <= (cursor_data->header.body_len - cursor_data->buff_offset)){
                            memcpy(cursor_data->read_buff + cursor_data->buff_offset, buf, recv_byte);
                            cursor_data->buff_offset += recv_byte;
                            if(cursor_data->buff_offset == cursor_data->header.body_len){
                                cursor_data->is_read_finished = 1;
                            }
                        }else{
                            php_error(E_WARNING,"Yar recive error format request");
                            close(sockfd);
                            client[i].fd = -1;
                            php_yar_listener_socket_clear_client_data(&(data->clients_data[i]));
                        }
                    }
                    
                    if(cursor_data->is_read_finished == 1){
                        temp = php_yar_packager_unpack(data->clients_data[i].read_buff, data->clients_data[i].header.body_len, err_msg TSRMLS_CC);
                        request = php_yar_request_unpack(temp);
                        zval_dtor(temp);            
                        response = php_yar_response_instance(TSRMLS_C);
                        php_yar_listener_socket_exec(self, request, response);
                        php_yar_listener_socket_response(sockfd, request, response TSRMLS_CC);
                        php_yar_request_destroy(request TSRMLS_CC);
                        php_yar_response_destroy(response TSRMLS_CC);
                        
                        php_yar_listener_socket_clear_client_data(&(data->clients_data[i]));
                    }
                }

                if (--nready <= 0){
                    break;
                }
            }
        }
    }
        
    for(i=1;i < YAR_LISTENER_SOCKET_CLIENT_OPEN_MAX; i++){
        client[i].fd = -1;
        php_yar_listener_socket_clear_client_data(&(data->clients_data[i]));
    }
    
    return 1;
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
    close(data->listenfd);
    data->listenfd = 0;
    return;
} 
/* }}} */

yar_listener_interface_t * php_yar_listener_socket_init(TSRMLS_D) /* {{{ */ {
    yar_listener_interface_t *self;
    yar_listener_data_t *data = (yar_listener_data_t *)emalloc(sizeof(yar_listener_data_t));
    
    self = ecalloc(1, sizeof(yar_listener_interface_t));
    self->data = (void *)data;
    
    self->executor = NULL;
    
    self->handle = php_yar_listener_socket_handle;
    self->close = php_yar_listener_socket_close;
    
    return self;
} 
/* }}} */

void php_yar_listener_socket_destroy(yar_listener_interface_t *self TSRMLS_DC) /* {{{ */ {
    yar_listener_data_t *data = (yar_listener_data_t *)self->data;
    if(data->listenfd){
        close(data->listenfd);
        data->listenfd = 0;
    }
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
