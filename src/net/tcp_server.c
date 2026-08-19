// TODO: 增加更多 类型返回错误处理
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
#include "other/prime.h"

#define DEBUG_TCP_SERVER(...) DEBUG(DEBUG_FLAG_TCPSERVER, ##__VA_ARGS__)

#define TCP_SERVER_CONNECTION_COUNT 8192
#define TCP_SERVER_MAX_EVENTS 8192
#define TCP_SERVER_HOSTNAME_LEN 64

#define event_is_error(e) (e)->events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)

typedef struct{
    int listen_fd; // 监听socket
    int epoll_fd; // epoll实例
    
    // 连接管理
    void **event_data;

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
int tcp_server_run(tcpServer* server);
void tcp_server_close_connection(tcpServer* server, connection_t *conn);

static inline int epoll_ctl_add_ptr(int epfd, int fd, uint32_t events, void* ptr){
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = ptr; 
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

#define epoll_ctl_add(epfd, fd, events) epoll_ctl_add_ptr(epfd, fd, events, NULL)

// 初始化和生命周期
tcpServer* tcp_server_create(const char* host, int port){
    tcpServer* server = (tcpServer *)malloc(sizeof(tcpServer));

    // key : fd -> value : connection 的映射 
    server->event_data = malloc(sizeof(void *) * TCP_SERVER_MAX_EVENTS);
    server->port = port;
    snprintf(server->host, sizeof(server->host), "%s", host);
    server->running = false;
    server->max_connections = TCP_SERVER_CONNECTION_COUNT;

    return server;
}

void tcp_server_destroy(tcpServer *server){
    close(server->epoll_fd);
    close(server->listen_fd);
    free(server->event_data);
    free(server);
    return;    
}

/* 启动服务器 返回0表示正确 或者 def.h中的错误码 */
int tcp_server_start(tcpServer* server) {
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 原代码：bind()
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(server->port);
    
    bind(server->listen_fd, (struct sockaddr*)&local, sizeof(local));
    listen(server->listen_fd, SOMAXCONN);
    server->epoll_fd = epoll_create1(0);

    epoll_ctl_add(server->epoll_fd, server->listen_fd, EPOLLIN | EPOLLOUT | EPOLLET);
    server->running = true;

    return 0;
}


/* 服务器运行 */
int tcp_server_run(tcpServer* server){
    struct sockaddr_in cli_addr;
    int socklen = sizeof(cli_addr);
    Arena arena_global = {0}; // 全局内存分配器

    struct epoll_event event_array[TCP_SERVER_MAX_EVENTS];

    while (1){
        int event_count = epoll_wait(server->epoll_fd, event_array, TCP_SERVER_MAX_EVENTS, -1);
        
        for (int i = 0; i < event_count; i ++){
            connection_t* conn = event_array[i].data.ptr;
            int fd = conn ? conn->fd : server->listen_fd;

            if (fd == server->listen_fd){ // 如果是服务器fd
                if (event_is_error(&event_array[i])){
                    DEBUG_TCP_SERVER("listen fd error\n");
                    continue; // 服务器fd损坏就跳过
                }

                server_handle(server);
            }else{ // 如果是其他事件fd
                if (event_is_error(&event_array[i])){
                    tcp_server_close_connection(server, conn); // 其中会关闭fd
                    continue;
                }


            }
        }
    }
}

/* 服务器关闭一个connction_t连接并且清空它，调用connection_t的清空手段 */
void tcp_server_close_connection(tcpServer* server, connection_t* conn){
    
}

