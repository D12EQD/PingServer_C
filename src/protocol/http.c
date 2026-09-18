#define _GNU_SOURCE
#include <sys/stat.h>
#include <ctype.h>
#include <stdint.h>
#include <stdnoreturn.h>

#include "protocol/picohttpparser.h"
#include "protocol/http.h"
#include "protocol/protocol.h"

#include "ds/ping_arena.h"
#include "ds/str.h"
#include "ds/buffer.h"

#include "other/debug.h"
#include "other/def.h"
#include "net/connection.h"

#include "route/router.h"

#define DEFAULT_HTTP_RESPONSE_HEADER_SIZE 16
#define DEBUG_HTTP(...) DEBUG(DEBUG_FLAG_HTTP, ##__VA_ARGS__)
#define DEBUGC_HTTP(...) DEBUG_CHAR(DEBUG_FLAG_HTTP, LV_INFO, ##__VA_ARGS__) 

static const char* get_reason_phrase(int status) {
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

static inline void print_len_str(const char *s, size_t len){
    DEBUGC_HTTP("%.*s", (int)len, s);
}

static inline void http_req_print(httpRequest* req){
    DEBUG_HTTP(LV_INFO, "================ HTTP REQUEST ============== \n");

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

static inline int http_init(Connection* conn){
    HttpContext * ctx = arena_alloc_ref_block(conn->arena, sizeof(HttpContext));

    if (ctx == NULL){
        return ERROR_MEM_FULL;
    }
    memset(ctx, 0 ,sizeof(HttpContext));

    conn->protocol_ctx = ctx;
    return 0;
}

protocolHandler http_protocol_handler_1_1 = {0};

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

/*
 * 读取 http 报文内容，conn 应完成接受tcp数据的内容
*/
int http_protocol_read(Connection* conn){
    DEBUG_HTTP(LV_INFO, "http_protocol_read start\n");
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
            DEBUG_HTTP(LV_INFO, "http parse error\n");
            return ERROR_HTTP_PARSE;
        }

        DEBUG_HTTP(LV_INFO, "http request need more data\n");
        req->last_len = phr_ret;
        return ERROR_HTTP_NEED_MORE;
    }

    http_req_print(req);
    req->last_len = 0;
    return 0;
}

/*
 * process http request and send http response 
 * return: TCP_PROTO_xxx
 * */
int http_protocol_process(Connection *conn){
    HttpContext* ctx = conn->protocol_ctx;
    httpRequest* req = &ctx->http_req;
    httpResponse* res = &ctx->http_res;
    
    // init 
    conn->is_keep_alive = req->minor_version; // 1 就是支持长连接，否则就是短链接
    res->body_fd = -1;
    res->send_st = HTTP_SEND_WAIT;

    if (http_protcol_check(conn) != 0){ // 预防处理
        DEBUG_HTTP(LV_WARN, "error http parse\n");
        return TCP_PROTO_CLOSE;
    }

    routerFunction r_func = get_router_function(req->path, req->path_len, req->method, req->method_len); // 调用router层进行业务处理
    r_func(conn); // 交付给router就是正确
    
    DEBUG_HTTP(LV_INFO, "router finish\n");
    return TCP_PROTO_SEND;
}

int http_protocol_close(Connection *conn){
    DEBUG_HTTP(LV_INFO, "http closing start\n");
    if (!conn->protocol_ctx) return 0;
    HttpContext  *ctx   = conn->protocol_ctx;
    httpResponse *res   = &ctx->http_res;
    MemoryArena  *arena = conn->arena;
    Buffer* buf = conn->send_buf;
    buffer_clean(buf);

    int n = snprintf((char *)buf->data, buf->cap,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: %s\r\n"
        "\r\n", 
        // args
        200, get_reason_phrase(200),
        "text/plain; charset=utf-8", 
        0,
        "close"
    );
    buf->len += n;
    
    while (1){
        int ret = connection_send(conn, NULL, 0);
        if (ret == CONN_SEND_DONE) break;
        if ( unlikely(ret == CONN_SEND_ERROR) ){
            arena_recycle_ref(arena, conn->protocol_ctx);
            return HTTP_SEND_ERROR;
            break;
        }
    }
    
    arena_recycle_ref(arena, conn->protocol_ctx);
    DEBUG_HTTP(LV_INFO, "http closing end\n");
    return 0;
}

int http_protocol_write(Connection *conn){
    DEBUG_HTTP(LV_INFO, "http write data\n");
    HttpContext  *ctx = conn->protocol_ctx;
    httpResponse *res = &ctx->http_res;
    Buffer *buf = conn->send_buf;

    switch (res->send_st) {
        case HTTP_SEND_WAIT:
            buffer_clean(buf);
            res->send_st = HTTP_SEND_HEAD;
            break;
        case HTTP_SEND_HEAD:
            goto send_head; 
        case HTTP_SEND_BODY:
            goto send_body;
    }

    {
        size_t body_len = 0;
        if (res->body_fd){
            struct stat sb = {0};
            fstat(res->body_fd, &sb);
            body_len = sb.st_size;
        }else{
            body_len = strlen(res->body);
        }
        res->body_len = body_len;
    }

    // TODO : use String, and add more headers, such as : keep-alive ...
    int n = snprintf((char *)buf->data, buf->cap,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        res->status_code, get_reason_phrase(res->status_code),
        res->content_type ? res->content_type : "text/plain; charset=utf-8", 
        res->body_len,
        conn->is_keep_alive ? "keep-alive" : "close"
    );
    buf->len += n;

    

send_head:
    int ret = connection_send(conn, NULL, 0);
    if ( unlikely(ret == CONN_SEND_PARTIAL) ){
        return HTTP_SEND_HEAD;
    }else if ( unlikely(ret == CONN_SEND_ERROR) ){
        res->send_st = HTTP_SEND_ERROR;
        return CONN_SEND_ERROR; 
    }

    res->send_st = HTTP_SEND_BODY;

    if (res->body_fd <= 0){ // 处理如果需要传输 字符串的情况
        buffer_clean(buf);
        if (unlikely(buf->cap < res->body_len)) return HTTP_SEND_ERROR;
        memcpy(buf->data, res->body, res->body_len);
        buf->len = res->body_len;
    }
send_body:
    // 发送body
    if (res->body_fd > 0) {
        ret = connection_send_fd(conn, res->body_fd, &res->body_off , res->body_len);
        if (ret == CONN_SEND_ERROR) return HTTP_SEND_ERROR;
        else if (ret == CONN_SEND_PARTIAL) return CONN_SEND_PARTIAL;
    } else if (res->body){
        ret = connection_send(conn, NULL, 0);
        if ( unlikely(ret == CONN_SEND_PARTIAL) ){
            return HTTP_SEND_HEAD;
        }else if ( unlikely(ret == CONN_SEND_ERROR) ){
            res->send_st = HTTP_SEND_ERROR;
            return CONN_SEND_ERROR; 
        }
    }
    return CONN_SEND_DONE;
}

/* RFC 3986 pchar + 查询串常见字符 */
static inline int is_valid_path_char(unsigned char c) {
    if ((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9')) return 1;
    switch (c) {
        case '-': case '_': case '~': case '.':
        case '/':
        case '%':
        case '?': case '&': case '=':
        case ':': case '@':
        case '!': case '$': case '\'': case '(':
        case ')': case '*': case '+': case ',':
        case ';': case '#':
            return 1;
        default:
            return 0;
    }
}

// 路径安全: 必须以 / 开头, 无控制字符, 无 .. 穿越, 合法百分号编码 
static int is_safe_path(const char *p, size_t len) {
    if (len == 0 || len > 2048) return 0;
    if (p[0] != '/') return 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)p[i];

        if (c < 0x20 || c == 0x7f) return 0;
        if (!is_valid_path_char(c)) return 0;

        if (i && p[i] == '.' && p[i-1] == '.') return 0;

        if (c == '%') {
            if (i + 2 >= len) return 0;
            if (!isxdigit((unsigned char)p[i+1]) ||
                !isxdigit((unsigned char)p[i+2])) return 0;
            i += 2;
        }
    }
    return 1;
}

// 大小写不敏感比较  
static int ci_equal(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return 0;
    }
    return 1;
}

// 在 headers 中查找, 返回索引, 未找到 -1  
static int find_header(const httpRequest *req, const char *name, size_t name_len) {
    for (size_t i = 0; i < req->num_headers; i++) {
        if (req->headers[i].name_len == name_len &&
            ci_equal(req->headers[i].name, name, name_len)) {
            return (int)i;
        }
    }
    return -1;
}
 
static long parse_content_length(const char *s, size_t n) {
    if (n == 0 || n > 18) return -1;
    long v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

int http_protcol_check(Connection *conn) {
    HttpContext  *ctx = conn->protocol_ctx;
    if (unlikely(!ctx)) return ERROR_HTTP_PARSE;

    httpRequest *req = &ctx->http_req;

    if (req->minor_version != 0 && req->minor_version != 1)
        return ERROR_HTTP_PARSE;

    if (req->minor_version == 1 &&
        find_header(req, "Host", 4) < 0)
        return ERROR_HTTP_PARSE;


    int is_get = 0, is_head = 0;
    if      (req->method_len == 3 && memcmp(req->method, "GET",  3) == 0) is_get  = 1;
    else return ERROR_HTTP_PARSE;

    /* --- 3. 路径 --- */
    if (!is_safe_path(req->path, req->path_len))
        return ERROR_HTTP_PARSE;
    

    /* --- 4. 头部名合法性 (RFC 7230 tchar) --- */
    for (size_t i = 0; i < req->num_headers; i++) {
        const unsigned char *n = (const unsigned char *)req->headers[i].name;
        size_t nl = req->headers[i].name_len;
        if (nl == 0) return ERROR_HTTP_PARSE;
        for (size_t j = 0; j < nl; j++) {
            unsigned char c = n[j];
            if (!(isalnum(c) || strchr("!#$%&'*+-.^_`|~", c)))
                return ERROR_HTTP_PARSE;
        }
    }


    /* --- 5. 请求走私防御: CL 与 TE 不能同时出现 --- */
    int cl_idx = find_header(req, "Content-Length",    14);
    int te_idx = find_header(req, "Transfer-Encoding", 17);
    if (cl_idx >= 0 && te_idx >= 0)
        return ERROR_HTTP_PARSE;


    if (is_get || is_head) {
        if (cl_idx >= 0) {
            long cl = parse_content_length(
                req->headers[cl_idx].value, req->headers[cl_idx].value_len);
            if (cl < 0 || cl != 0) return ERROR_HTTP_PARSE;
        }
        if (req->body_len != 0) return ERROR_HTTP_PARSE;
    }

    return 0;
}

// tcp服务器不再需要http服务
void tcpserver_close_http(){
}

