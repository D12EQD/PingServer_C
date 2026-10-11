#pragma once

#include <stddef.h>
#include "net/connection.h" 
#include "net/tcp_server.h"

typedef void (*routerFunction)(Connection*); // 该函数返回0则是正常，否则返回ERROR错误

#ifndef ROUTER_FILE_CACHE_LIMIT
    #define ROUTER_FILE_CACHE_LIMIT 32
#endif

enum HttpMethod{
    MethodGet,
};

enum HttpMatchRule{
    MatchPrefix,
    MatchExact
};

typedef struct{
    char *path;
    routerFunction func;
    int method;
    int match_rule;
}Router;


routerFunction get_router_function(const char *path, size_t path_len, const char *method, size_t method_len);
void router_layer_init(tcpServer * server);
