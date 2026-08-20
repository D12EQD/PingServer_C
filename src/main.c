#include <stdio.h>

#include "ds/linklist.h"
#include "other/debug.h"
#include "net/tcp_server.h"

#define N 1000

int main(){
    tcpServer *server = tcp_server_create("test.com", 8080);
    tcp_server_start(server);

    tcp_server_destroy(server);
    printf("finish\n");
}