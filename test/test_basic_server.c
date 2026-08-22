#include <stdio.h>
#include "other/debug.h"
#include "net/tcp_server.h"

#define N 1000

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