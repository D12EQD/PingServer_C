#pragma once
#include <sys/epoll.h>

#include "ds/arena.h"

#include "net/connection.h"

#include "other/debug.h"

#define DEBUG_TCP_SERVER(...) DEBUG(DEBUG_FLAG_TCPSERVER, ##__VA_ARGS__)
#define TCP_SERVER_HOSTNAME_LEN 64

#define event_is_error(e) (e)->events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)

typedef struct{
    int listen_fd; // 监听socket
    int epoll_fd; // epoll实例
 
    // 配置
    char host[TCP_SERVER_HOSTNAME_LEN];
    int port;
    uint32_t max_connections;
    
    // 状态
    volatile int running;             // 运行标志
    struct {
        uint64_t total_connections;
        uint64_t current_connections;
    } stats;
}tcpServer;


tcpServer* tcp_server_create(const char* host, int port);
int tcp_server_start(tcpServer* server);
void tcp_server_destroy(tcpServer* server);
int tcp_server_run(tcpServer* server);
void tcp_server_close_connection(tcpServer* server, connection_t *conn);

int server_handle_accept_event(tcpServer* server, struct epoll_event* event, connection_t *conn_array, Arena *arena, struct epoll_event* event_array);
int server_handle_event(tcpServer* server, connection_t *conn, Arena *arena, struct epoll_event* event);
