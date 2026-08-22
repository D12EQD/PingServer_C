#pragma once
#include "net/connection.h"

typedef struct{
    int (*on_read)(connection_t *conn);
    int (*on_process)(connection_t *conn);
    int (*on_write)(connection_t *conn);
    int (*on_close)(connection_t *conn);
    char *name;
}protocolHandler;

typedef struct{
    protocolHandler* handler;
    void* protocol_temp; // 暂存内存，保存协议相关信息，不同协议有不同的解释方式
} protocolContext;

void connection_clear_protocol(connection_t *conn);
int connction_get_protocol_ctx(connection_t *conn);
// void connection_set_protocol(connection_t *conn, protocolHandler *handler, void *temp);
