#ifndef TCP_SERVER_CONNECTION_COUNT
    #define TCP_SERVER_CONNECTION_COUNT 10
#endif

#ifndef TCP_SERVRE_DEFAULT_OUT_TIME
    #define TCP_SERVRE_DEFAULT_OUT_TIME 1000
#endif

#ifndef PING_ARENA_LIMIT_BLOCK
    #define PING_ARENA_LIMIT_BLOCK 64
#endif

#include <stdio.h>
#include "other/debug.h"
#include "net/tcp_server.h"

int main(){
    DEBUG_FLAG_SET(DEBUG_FLAG_ALL);

    tcpServer *server = tcp_server_create("test.com", 8080);
    if (tcp_server_start(server) < 0){
        printf("error\n");
        return 0;
    }

    tcp_server_run(server);

    tcp_server_destroy(server);
    printf("finish\n");
}
