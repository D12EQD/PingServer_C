#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include "ds/ping_arena.h"
#include "ds/buffer.h"
#include "net/connection.h"

#include "net/event.h"
#include "other/debug.h"

#define DEBUG_TCP_SERVER(...) DEBUG(DEBUG_FLAG_TCPSERVER, ##__VA_ARGS__)
#define TCP_SERVER_HOSTNAME_LEN 64

#define event_is_error(e) (e)->events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)

typedef struct{
    Connection * conn_array;
    EventTcpContext* event_tcp_array;
    EventTimerContext* event_timer_array;
    struct {
        uint64_t total_connections;
        uint64_t current_connections;
    } stats;
    int listen_fd; // 监听socket
    int epoll_fd; // epoll实例

    // 配置
    char host[TCP_SERVER_HOSTNAME_LEN];
    int port;
    uint32_t max_connections;

    // 状态
    volatile int running;             // 运行标志
}tcpServer;

tcpServer* tcp_server_create(const char* host, int port);
int tcp_server_start(tcpServer* server);
void tcp_server_destroy(tcpServer* server);
int tcp_server_run(tcpServer* server);
void tcp_server_close_connection(tcpServer* server, Connection *conn);

void tcpserver_listen_on_read(void *temp_ctx);
void tcpserver_listen_on_error(void *temp_ctx);
void tcpserver_tcp_on_read(void * temp_ctx);
void tcpserver_tcp_on_error(void * temp_ctx);
void tcpserver_tcp_on_write(void * temp_ctx);
