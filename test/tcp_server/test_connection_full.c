#ifndef TCP_SERVER_CONNECTION_COUNT
    #define TCP_SERVER_CONNECTION_COUNT 2048
#endif

#ifndef TCP_SERVRE_DEFAULT_OUT_TIME
    #define TCP_SERVRE_DEFAULT_OUT_TIME 1000
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "other/debug.h"
#include "net/tcp_server.h"

#define N 1000

int main(){
    DEBUG_FLAG_SET(DEBUG_FLAG_ALL);
    DEBUG_LEVEL_SET(LV_ALL);

    debug_log_fp = fopen("test/debug.txt", "w");

    tcpServer *server = tcp_server_create("127.0.0.1", 8080);
    if (tcp_server_start(server) < 0){
        printf("error\n");
        return 0;
    }

    tcp_server_run(server);

    tcp_server_destroy(server);
    printf("tcp sever error. check the debug.txt now!\n");
}
