#define _GNU_SOURCE

#include <stdio.h>

#include <stdint.h>
#include <string.h>

#include "protocol/picohttpparser.h"
#include "protocol/http.h"
#include "protocol/protocol.h"

#include "ds/buffer.h"

#include "other/debug.h"
#include "other/def.h"
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

/**
 * @brief Get the http protocol handler 1 1 object
 * @return protocolHandler* 
 */
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

int http_protocol_process(Connection *conn){
    return 0;
}
int http_protocol_read(Connection* conn){
    return 0;
}
int http_protocol_close(Connection *conn){
    return 0;
}
int http_protocol_write(Connection *conn, void* res){

    return 0;
}
int http_protcol_check(Connection * conn, httpRequest* req){

    return 0;
}
