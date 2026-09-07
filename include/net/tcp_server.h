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
void tcp_server_close_connection(tcpServer* server, Connection *conn);

// int server_handle_time_event(tcpServer* server, struct epoll_event* event, Connection *conn_array, MemoryArena *arena, struct epoll_event* event_array);


int server_handle_accept_event(
    tcpServer* server, 
    MemoryArena *arena, 
    struct epoll_event* listen_event,
    int new_conn_sock_fd,
    Connection *new_conn, 
    struct epoll_event* new_event,
    Event * new_event_data
);

int server_handle_tcp_event(tcpServer* server, Connection *conn, MemoryArena *arena, struct epoll_event* event);
