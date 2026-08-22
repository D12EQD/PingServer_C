#define _GNU_SOURCE
#include "protocol/protocol.h"
#include "net/connection.h"

#include <stdio.h>

#include <stdint.h>
#include <string.h>

#include "protocol/picohttpparser.h"
#include "protocol/http.h"

#include "ds/buffer.h"
#include "other/debug.h"
#include "other/def.h"

#define DEBUG_HTTP(...) DEBUG(DEBUG_FLAG_HTTP, ##__VA_ARGS__)

protocolHandler http_protocol_handler = {0};

/*
* 检查是否是标准的http数据，返回错误码或者是http总长度
*/
int http_protcol_check(connection_t * conn, httpRequest* req){
    buffer_t *buf = conn->read_buf;
    uint8_t *buf_read = buffer_peek(buf);
    uint32_t buf_read_size = buffer_read_cap(buf);

    if (buf_read_size <= 0) return ERROR_BUFFER_EMPTY;
    
    req->num_headers = sizeof(req->headers) / sizeof(req->headers[0]);

    int header_len = phr_parse_request(
        (void *)buf_read, buf_read_size,
        &req->method, &req->method_len,
        &req->path, &req->path_len,
        &req->minor_version,
        req->headers, &req->num_headers,
        0
    );

    if (header_len == -2) return ERROR_PROTO_NEED_MORE;
    if (header_len == -1){
        DEBUG_HTTP("http pares error\n");
        return ERROR_PROTO;
    }

    size_t content_length = 0;
    for (size_t i = 0; i < req->num_headers; i ++) {
        if (strncasecmp(req->headers[i].name, "Content-Length", req->headers[i].name_len) == 0) {
            content_length = strtoul(req->headers[i].value, NULL, 10);
            break;
        }
    }

    size_t total_request_len = header_len + content_length;

    if (buf_read_size < total_request_len) {
        return ERROR_PROTO_NEED_MORE; 
    }

    req->body = (void *)(buf_read + header_len);
    req->body_len = content_length;

    return total_request_len;
}

int http_protocol_process(connection_t *conn){
    DEBUG_HTTP("http_protocol_process start\n");    
    /*
        TDOO : 在这里进行路由分配和返回数据之类的东西 增加路由检测功能
    */

    return 0;
}

int http_protocol_close(connection_t *conn){
    protocolContext * ctx = conn->protocol_ctx;
    ctx->protocol_temp = NULL;
    ctx->handler = NULL;
    return 0;
}

/*
* 读取http报文，读取判断为正确的http报文后，将httpRequest放在protocol_temp中
* 返回错误码
*/
int http_protocol_read(connection_t* conn){
    httpRequest* req = (httpRequest*) arena_alloc(conn->arena, sizeof(httpRequest));

    int total_request_len = http_protcol_check(conn, req);
    if (total_request_len < 0) return total_request_len;

    DEBUG_HTTP(
        "http_protocol_read get a HTTP Request: %.*s %.*s HTTP/1.%d\n", 
        (int)req->method_len, req->method, 
        (int)req->path_len, req->path, 
        req->minor_version
    );

    protocolContext * ctx = conn->protocol_ctx;
    ctx->protocol_temp = req; 

    return 0;
}

protocolHandler* get_http_protocol_handler(){
    if (http_protocol_handler.name == NULL){
        http_protocol_handler.on_process = http_protocol_process;
        http_protocol_handler.on_read = http_protocol_read;
        http_protocol_handler.on_close = http_protocol_close;
        http_protocol_handler.on_write = NULL; // TODO
        http_protocol_handler.name = "HTTP/1.1\0";
    }
    return &http_protocol_handler;
}

