#define _GNU_SOURCE

#include <stdio.h>

#include <stdint.h>
#include <string.h>

#include "protocol/picohttpparser.h"
#include "protocol/http.h"
#include "protocol/protocol.h"

#include "ds/arena.h"
#include "ds/buffer.h"

#include "other/debug.h"
#include "other/def.h"

#include "route/router.h"

#include "net/connection.h"

#define DEFAULT_HTTP_RESPONSE_HEADER_SIZE 16
#define DEBUG_HTTP(...) DEBUG(DEBUG_FLAG_HTTP, ##__VA_ARGS__)

protocolHandler http_protocol_handler_1_1 = {0};

char error_404[] = "Not Found\n";
char server_name[] = "nginx";

static char* get_reason_phrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default: return "Unknown";
    }
}

/*
* 注意该操作可能会改变resp->headers的指针，使用时需要注意
*/
static inline void http_add_header(
    connection_t* conn, 
    httpResponse* resp, 
    char *name, int name_len, 
    char *value, int value_len)
{
    if (resp->cap_headers == resp->num_headers){
        size_t new_cap = resp->cap_headers / 2 + resp->cap_headers + 1;
        resp->headers = arena_realloc(
            conn->arena, 
            resp->headers, 
            resp->cap_headers * sizeof(struct phr_header), 
            new_cap * sizeof(struct phr_header)
        );
    }
    
    resp->headers[resp->num_headers].name = name;
    resp->headers[resp->num_headers].name_len = name_len;
    resp->headers[resp->num_headers].value = value;
    resp->headers[resp->num_headers].value_len = value_len;
    resp->num_headers ++;
}

/*
* 设置响应头，如果已存在同名字段则替换值，否则追加
*/
static inline void http_set_header(
    connection_t* conn, 
    httpResponse* resp, 
    char *name, int name_len, 
    char *value, int value_len)
{
    for (size_t i = 0; i < resp->num_headers; i++) {
        if (resp->headers[i].name_len == (size_t)name_len && 
            strncasecmp(resp->headers[i].name, name, name_len) == 0) {
            // 找到同名，直接替换值（保持 name 不变）
            resp->headers[i].value = value;
            resp->headers[i].value_len = value_len;
            return;
        }
    }

    // 未找到，走追加逻辑
    http_add_header(conn, resp, name, name_len, value, value_len);
}

/*
* 初始化response的基本构造 
* 为resp分配内存并且为其中的phr_headers数组分配一定大小
*/
static inline int http_response_init(connection_t* conn, httpResponse* resp){
    if (!conn || !resp) return ERROR_INVAILED;
    
    resp->minor_version = 1;   // HTTP/1.1
    resp->status = 200;
    resp->head_buf = buffer_create_from_arena(200, conn->arena); 
    resp->msg = get_reason_phrase(resp->status);
    resp->msg_len = strlen(resp->msg);
    
    resp->cap_headers = DEFAULT_HTTP_RESPONSE_HEADER_SIZE;
    resp->num_headers = 0;
    resp->headers = arena_alloc(conn->arena, sizeof(struct phr_header) * resp->cap_headers);
    resp->content_len_str = arena_alloc(conn->arena, sizeof(char) * 10);

    // 默认添加 Server 头
    http_add_header(conn, resp, "Server", strlen("Server"), server_name, strlen(server_name));
    return 0;
}

/*
* 初始化error response的基本构造 
* 根据 status 来选择不同的message报文
* 将conn->write_buf 作为需要发送错误数据
*/
// static inline int http_error_response(connection_t* conn, httpResponse* resp, int status){
//     if (!conn || !resp) return ERROR_INVAILED;

//     resp->status = status;
//     resp->msg = get_reason_phrase(resp->status);
//     resp->msg_len = strlen(resp->msg);

//     // 设置 Content-Type
//     http_set_header(
//         conn, 
//         resp, 
//         "Content-Type", strlen("Content-Type"), 
//         "text/plain", strlen("text/plain")
//     );

//     resp->msg = (void *)(conn->write_buf->data);
//     resp->msg_len = conn->write_buf->len;

//     return 0;
// }


/*
* 检查是否是标准的http数据，返回错误码或者是http总长度
*/
int http_protcol_check(connection_t * conn, httpRequest* req){
    DEBUG_HTTP("http_protcol_check start\n");

    buffer_t *buf = conn->read_buf;
    
    buffer_print(buf);
    uint8_t *buf_read = buffer_read_ptr(buf);
    uint32_t buf_read_size = buffer_readable(buf);

    if (buf_read_size <= 0){
        DEBUG_HTTP("http_protcol_check : ERROR_BUFFER_EMPTY\n");
        return ERROR_BUFFER_EMPTY;
    }
    
    req->num_headers = sizeof(req->headers) / sizeof(req->headers[0]);

    int header_len = phr_parse_request(
        (void *)buf_read, buf_read_size,
        &req->method, &req->method_len,
        &req->path, &req->path_len,
        &req->minor_version,
        req->headers, &req->num_headers, 0
    );

    if (header_len == -2){
        DEBUG_HTTP("http_protcol_check : ERROR_PROTO_NEED_MORE\n");
        return ERROR_PROTO_NEED_MORE;
    }
    if (header_len == -1){
        DEBUG_HTTP("http_protcol_check: ERROR_PROTO\n");
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
        DEBUG_HTTP("http_protcol_check : ERROR_PROTO_NEED_MORE\n");
        return ERROR_PROTO_NEED_MORE; 
    }

    req->body = (void *)(buf_read + header_len);
    req->body_len = content_length;

    DEBUG_HTTP("http_protcol_check : finish, it is ok\n");
    buffer_print(buf);
    
    return total_request_len;
}

int http_protocol_process(connection_t *conn){
    httpRequest* req = ((protocolContext *)(conn->protocol_ctx))->protocol_temp;
    if (!req) return ERROR_INVAILED;
    
    httpResponse* resp = (httpResponse*)arena_alloc(conn->arena, sizeof(httpResponse));
    http_response_init(conn, resp);

    const char *body = "Hello, World!\n";
    buffer_append(conn->write_buf, body, strlen(body));

    http_set_header(conn, resp, 
        "Content-Type", strlen("Content-Type"), 
        "text/plain", strlen("text/plain")
    );

    int ret = http_protocol_write(conn, resp);
    if (ret < 0) return ret;

    // routerFunction func = get_router_function(req->path, req->path_len, req->method, req->method_len);
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
    buffer_read(conn->read_buf, total_request_len);

    return 0;
}

/*
* httpResponse res_void 中输入头部信息
* conn->write_buf 为 content 
*/
int http_protocol_write(connection_t* conn, void *response){
    if (!response) return ERROR_INVAILED;

    httpResponse* resp = response;

    buffer_t *head_buf = resp->head_buf; buffer_reset(head_buf);

    // 设置Content-Length
    sprintf(resp->content_len_str, "%d", conn->write_buf->len); // 设置为conn write buffer
    http_set_header(
        conn, resp, 
        "Content-Length", strlen("Content-Length"), 
        resp->content_len_str, 
        strlen(resp->content_len_str)
    );

    
    int len = snprintf((void *)(buffer_write_ptr(head_buf)), buffer_writable(head_buf), 
        "HTTP/1.%d %d %s\r\n", resp->minor_version, resp->status, resp->msg
    );
    buffer_write(head_buf, len);

    for (size_t i = 0; i < resp->num_headers; i ++){
        struct phr_header *h = &resp->headers[i];
        size_t header_len = h->name_len + 2 + h->value_len + 2;
        
        if (buffer_writable(head_buf) < header_len){
            return ERROR_BUFFER_FULL;
        }

        buffer_append(head_buf, h->name, h->name_len);
        buffer_append(head_buf, ": ", 2);
        buffer_append(head_buf, h->value, h->value_len);
        buffer_append(head_buf, "\r\n", 2);
    }

    if (buffer_writable(head_buf) < 2) return ERROR_BUFFER_FULL;
    buffer_append(head_buf, "\r\n", 2);

    DEBUG_HTTP("send http head\n");
    connection_send(conn, head_buf);
    
    DEBUG_HTTP("send http content\n");
    connection_send(conn, NULL);

    return 0;
}

protocolHandler* get_http_protocol_handler_1_1(){
    if (http_protocol_handler_1_1.name == NULL){
        http_protocol_handler_1_1.on_process = http_protocol_process;
        http_protocol_handler_1_1.on_read = http_protocol_read;
        http_protocol_handler_1_1.on_close = http_protocol_close;
        http_protocol_handler_1_1.on_write = http_protocol_write; 
        http_protocol_handler_1_1.name = "HTTP/1.1\0";
    }
    return &http_protocol_handler_1_1;
}
