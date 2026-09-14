#define _GNU_SOURCE

#include <stdint.h>
#include <stdnoreturn.h>

#include "protocol/picohttpparser.h"
#include "protocol/http.h"
#include "protocol/protocol.h"

#include "ds/ping_arena.h"
#include "ds/buffer.h"
#include "other/debug.h"
#include "other/def.h"
#include "net/connection.h"

#define DEFAULT_HTTP_RESPONSE_HEADER_SIZE 16
#define DEBUG_HTTP(...) DEBUG(DEBUG_FLAG_HTTP, ##__VA_ARGS__)
#define DEBUGC_HTTP(...) DEBUG_CHAR(DEBUG_FLAG_HTTP, ##__VA_ARGS__) 

protocolHandler http_protocol_handler_1_1 = {0};

char error_404[] = "Not Found\n";
char server_name[] = "nginx";

static inline void print_len_str(const char *s, size_t len){
    DEBUGC_HTTP("%.*s", (int)len, s);
}

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

static inline void http_req_print(httpRequest* req){
    DEBUG_HTTP("================ HTTP REQUEST ============== \n");

    DEBUGC_HTTP(ANSI_GREEN "Method: " ANSI_RESET);
    print_len_str(req->method, req->method_len);
    DEBUGC_HTTP("\n");

    DEBUGC_HTTP(ANSI_CYAN "Path: " ANSI_RESET);
    print_len_str(req->path, req->path_len);
    DEBUGC_HTTP("\n");

    DEBUGC_HTTP(ANSI_YELLOW "Version: " ANSI_RESET "HTTP/1.%d\n", req->minor_version);

    // 头部
    DEBUGC_HTTP(ANSI_BLUE "Headers (%zu):" ANSI_RESET "\n", req->num_headers);

    for (size_t i = 0; i < req->num_headers; i++) {
        print_len_str(req->headers[i].name, req->headers[i].name_len); DEBUGC_HTTP(" : ");
        print_len_str(req->headers[i].value, req->headers[i].value_len); DEBUGC_HTTP("\n");
    }

    // Body
    DEBUGC_HTTP(ANSI_RED "Body (%zu bytes):" ANSI_RESET "\n", req->body_len);
    if (req->body && req->body_len > 0) {
        size_t print_len = req->body_len;
        const size_t MAX_PRINT = 1024;
        if (print_len > MAX_PRINT) {
            print_len = MAX_PRINT;
        }
        fwrite(req->body, 1, print_len, debug_log_fp);
        if (req->body_len > MAX_PRINT) {
            DEBUGC_HTTP("\n... (%zu more bytes)", req->body_len - MAX_PRINT);
        }
        DEBUGC_HTTP("\n");
    }

    DEBUGC_HTTP(ANSI_YELLOW "last_len: %zu" ANSI_RESET "\n", req->last_len);
}

/**
 * @brief Get the http protocol handler 1 1 object
 * @return protocolHandler* 
 */
protocolHandler* get_http_protocol_handler_1_1(){
    if (http_protocol_handler_1_1.name == NULL){
        http_protocol_handler_1_1.on_process = http_protocol_process; // tcp server use this to enter http server layer
        http_protocol_handler_1_1.on_read = http_protocol_read;
        http_protocol_handler_1_1.on_close = http_protocol_close;
        http_protocol_handler_1_1.on_write = http_protocol_write; 
        http_protocol_handler_1_1.name = "HTTP/1.1\0";
    }
    return &http_protocol_handler_1_1;
}

static inline int http_init(Connection* conn){
    HttpContext * ctx = arena_alloc_ref_block(conn->arena, sizeof(HttpContext));
    memset(ctx, 0 ,sizeof(HttpContext));

    if (ctx == NULL){
        return ERROR_MEM_FULL;
    }

    conn->protocol_ctx = ctx;
    return 0;
}

/*
 * 读取 http 报文内容，conn 应完成接受tcp数据的内容
*/
int http_protocol_read(Connection* conn){
    DEBUG_HTTP("http_protocol_read start\n");
    if (!conn->protocol_ctx){
        int ret = http_init(conn);
        if (ret < 0) return ret;
    }

    HttpContext *ctx = conn->protocol_ctx;
    httpRequest* req = &(ctx->http_req); 

    Buffer* buf = conn->read_buf;
    req->num_headers = sizeof(req->headers) / sizeof(req->headers[0]);

    int phr_ret = phr_parse_request(
        (char *)buf->data, 
        buf->len, 
        &(req->method) , &(req->method_len), 
        &(req->path), &(req->path_len), 
        &(req->minor_version),
        (req->headers), &(req->num_headers), (req->last_len)
    );

    if (unlikely(phr_ret < 0)){
        if (phr_ret != -2){
            DEBUG_HTTP("http parse error\n");
            return ERROR_HTTP_PARSE;
        }

        DEBUG_HTTP("http request need more data\n");
        req->last_len = phr_ret;
        return ERROR_HTTP_NEED_MORE;
    }

    http_req_print(req);
    req->last_len = 0;
    return 0;
}

/*
 * process http request and send http response 
 * return:
 * - 0 close protocol
 * - 1 send the send_buf of connection
 * - 2 waiting and do not close
 * < 0 error
 * */
int http_protocol_process(Connection *conn){
    if (conn->send_buf->len != 0){
        return CONN_PROTO_CLOSE;
    }
    http_protocol_write(conn);
    return CONN_PROTO_SEND;
}

int http_protocol_close(Connection *conn){
    DEBUG_HTTP("http closing start\n");
    if (conn->protocol_ctx) arena_recycle_ref(conn->arena, conn->protocol_ctx);
    DEBUG_HTTP("http closing end\n");
    return 0;
}

int http_protocol_write(Connection *conn){
    DEBUG_HTTP("http write data\n");
    Buffer *buf = conn->send_buf;

    const char *body = "Hello, PingNet!\n";
    size_t body_len = strlen(body);

    // 拼装 HTTP 响应
    int n = snprintf(
        (char *)buf->data + buf->len,          // 从已写位置追加
        buf->cap - buf->len,                   // 剩余空间
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"                
        "\r\n"
        "%s",
        body_len, body
    );

    if (n < 0) return -1;
    if ((size_t)n >= buf->cap - buf->len) {
        return -1;
    }

    buf->len += (uint32_t)n;
    return n;
}

int http_protcol_check(Connection * conn, httpRequest* req){

    return 0;
}
