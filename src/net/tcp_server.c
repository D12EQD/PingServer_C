#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include "ds/hash_table.h"
#include "net/connection.h"
#include "other/def.h"
#include "other/debug.h"

#define DEBUG_TCP_SERVER(...) DEBUG(DEBUG_FLAG_TCPSERVER, ##__VA_ARGS__)

#define TCP_SERVER_CONNECTION_COUNT 8192
#define TCP_SERVER_HOSTNAME_LEN 64

typedef struct{
    int listen_fd; // 监听socket
    int epoll_fd; // epoll实例
    
    // 连接管理
    hashTable* conn_hash; // fd -> connection_t* 映射
    
    // 配置
    char host[TCP_SERVER_HOSTNAME_LEN];
    int port;
    int max_connections;
    
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
int tcp_server_run(tcpServer* server, int timeout_ms);
void tcp_server_close_connection(tcpServer* server, int conn_id);
int tcp_server_send(tcpServer* server, int conn_id, const char* data, size_t len);

// 初始化和生命周期
tcpServer* tcp_server_create(const char* host, int port){
    tcpServer* server = (tcpServer *)malloc(sizeof(tcpServer));

    // key : fd -> value : connection 的映射 
    server->conn_hash = hash_create(TCP_SERVER_CONNECTION_COUNT, sizeof(connection_t));
    server->port = port;
    memcpy(server->host, host, min(strlen(host), TCP_SERVER_HOSTNAME_LEN) * sizeof(char));
    server->running = false;
    server->max_connections = TCP_SERVER_CONNECTION_COUNT;

    return server;
}

void tcp_server_destroy(tcpServer *server){
    close(server->epoll_fd);
    close(server->listen_fd);
    hash_free(server->conn_hash);
    free(server);
    return;    
}

/*
* 服务器启动
*/
int tcp_server_start(tcpServer* server) {
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (server->listen_fd < 0) {
        DEBUG_TCP_SERVER("tcp_server_start failed: socket fd create failed\n");
        return -1;
    }
    
    // 原代码：setsockopt()
    int opt = 1;
    if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        DEBUG_TCP_SERVER("tcp_server_start failed : setsockopt");
        return -1;
    }
    
    // 原代码：bind()
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(server->port);
    
    if (bind(server->listen_fd, (struct sockaddr*)&local, sizeof(local)) < 0) {
        DEBUG_TCP_SERVER("tcp_server_start failed : bind");
        return -1;
    }
    
    if (listen(server->listen_fd, SOMAXCONN) < 0) {
        DEBUG_TCP_SERVER("tcp_server_start failed : listen");
        return -1;
    }
    
    server->epoll_fd = epoll_create1(0);
    if (server->epoll_fd < 0) {
        DEBUG_TCP_SERVER("tcp_server_start failed : epoll_create1");
        return -1;
    }

    epoll_event_register(server, server->listen_fd, EPOLLIN);
    server->running = true;

    return 0;
}

void tcp_server_destroy(tcpServer* server);

// 事件循环（核心）
int tcp_server_run(tcpServer* server, int timeout_ms);

// 连接管理
void tcp_server_close_connection(tcpServer* server, int conn_id);

// 数据发送（可能需要异步缓冲）
int tcp_server_send(tcpServer* server, int conn_id, const char* data, size_t len);
