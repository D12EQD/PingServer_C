#pragma once
#include "protocol/picohttpparser.h"
#include "net/connection.h"
#include "protocol/protocol.h"

#define HTTP_HAED_NUMBER_LIMIT 32
typedef void* (protocolCallback)(void *);

typedef struct {
    const char *method;
    size_t method_len;
    
    const char *path;
    size_t path_len;
    
    int minor_version;
    
    struct phr_header headers[HTTP_HAED_NUMBER_LIMIT];
    
    size_t num_headers;
    const char *body;
    size_t body_len;
    size_t last_len;
} httpRequest;

typedef struct {
    char *msg;
    struct phr_header *headers;
    char *content_len_str;
    size_t num_headers;
    size_t cap_headers;
    size_t msg_len;
    int minor_version;
    int status;
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
int http_protcol_check(Connection * conn, httpRequest* req);
int http_protocol_process(Connection *conn);
