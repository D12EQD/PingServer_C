#pragma once
#include "net/connection.h"

typedef struct{
    int (*on_read)(Connection *conn);
    int (*on_process)(Connection *conn);
    int (*on_write)(Connection *conn, void *);
    int (*on_close)(Connection *conn);
    char *name;
}protocolHandler;

void connection_clear_protocol(Connection *conn);
int connection_get_protocol_ctx(Connection *conn);
// void connection_set_protocol(Connection *conn, protocolHandler *handler, void *temp);
