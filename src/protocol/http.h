#pragma once
#include <stdio.h>
#include "ds/str.h"
#include "protocol/picohttpparser.h"
#include "net/connection.h"
#include "protocol/protocol.h"

#define HTTP_HEADER_NUMBER_LIMIT 32
#define HTTP_HEADER_TABLE(X)                    \
    /* 请求常用 */                              \
    X(HOST,             "Host")                 \
    X(USER_AGENT,       "User-Agent")           \
    X(ACCEPT,           "Accept")               \
    X(ACCEPT_ENCODING,  "Accept-Encoding")      \
    X(ACCEPT_LANGUAGE,  "Accept-Language")      \
    X(AUTHORIZATION,    "Authorization")        \
    X(CACHE_CONTROL,    "Cache-Control")        \
    X(CONNECTION,       "Connection")           \
    X(CONTENT_LENGTH,   "Content-Length")       \
    X(CONTENT_TYPE,     "Content-Type")         \
    X(REFERER,          "Referer")              \
    /* 响应常用 */                              \
    X(DATE,             "Date")                 \
    X(SERVER_NAME,      "Server")               \
    X(EXPIRES,          "Expires")              \
    X(LAST_MODIFIED,    "Last-Modified")        \
    X(ETAG,             "ETag")                 \
    X(LOCATION,         "Location")             \
    X(SET_COOKIE,       "Set-Cookie")           \
    X(CONTENT_ENCODING, "Content-Encoding")     \
    X(VARY,             "Vary")                 \
    X(ACCEPT_RANGES,    "Accept-Ranges")        \
    X(CONTENT_RANGE,    "Content-Range")


typedef enum {
#define X(id, str) HTTP_HEADER_##id,
    HTTP_HEADER_TABLE(X)
#undef X
    HTTP_HEADER_COUNT
} http_header_id;

extern char *const http_header_name_table[];

typedef void* (protocolCallback)(void *);
typedef struct {
    const char *method;
    size_t method_len;
    const char *path;
    size_t path_len;
    const char *body;
    size_t body_len;
    size_t num_headers;
    struct phr_header headers[HTTP_HEADER_NUMBER_LIMIT];
    size_t last_len;
    int minor_version;
} httpRequest;

typedef struct {
    const char* content_type;
    const char* body;
    size_t body_len;
    int status_code;
    int body_fd;
    off_t body_off;
    uint8_t send_st;
} httpResponse;

typedef struct {
    httpRequest http_req;
    httpResponse http_res;
} HttpContext;

extern protocolHandler http_protocol_handler;

protocolHandler* get_http_protocol_handler_1_1();

int http_protocol_process(Connection *conn);
int http_protocol_read(Connection* conn);
int http_protocol_close(Connection *conn);
int http_protocol_write(Connection *conn);
int http_protcol_check(Connection *conn);
int http_protocol_process(Connection *conn);
void tcpserver_close_http();
