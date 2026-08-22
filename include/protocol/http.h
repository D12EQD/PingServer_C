#pragma once
#include "protocol/picohttpparser.h"
#include "net/connection.h"
#include "protocol/protocol.h"

typedef void* (protocolCallback)(void *);

typedef struct {
    const char *method;
    size_t method_len;
    
    const char *path;
    size_t path_len;
    
    int minor_version;
    
    struct phr_header headers[32];
    
    size_t num_headers;
    const char *body;
    size_t body_len;
} httpRequest;

typedef struct {
    int minor_version;
    int status;

    const char *msg;
    size_t *msg_len;

    struct phr_header *headers;
    size_t *num_headers;
    size_t last_len;
} httpResponse;

extern protocolHandler http_protocol_handler;

protocolHandler* get_http_protocol_handler_1_1();

int http_protocol_process(connection_t *conn);
int http_protocol_read(connection_t* conn);
int http_protocol_close(connection_t *conn);
int http_protocol_write(connection_t *conn, void* res);
int http_protcol_check(connection_t * conn, httpRequest* req);
int http_protocol_process(connection_t *conn);