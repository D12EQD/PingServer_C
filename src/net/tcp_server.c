#define _GNU_SOURCE  
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>   

#include "ds/arena.h"

#include "net/connection.h"
#include "net/tcp_server.h"

#include "other/def.h"
#include "other/debug.h"

#include "protocol/protocol.h"


#define DEBUG_TCP_SERVER(...) DEBUG(DEBUG_FLAG_TCPSERVER, ##__VA_ARGS__)

#define TCP_SERVER_CONNECTION_COUNT 512
#define TCP_SERVER_MAX_EVENTS 512

#define event_check(e, flag) (e)->events & (flag)

tcpServer* tcp_server_create(const char* host, int port);
int tcp_server_start(tcpServer* server);
void tcp_server_destroy(tcpServer* server);
int tcp_server_run(tcpServer* server);
void tcp_server_close_connection(tcpServer* server, connection_t *conn);

int server_handle_event(tcpServer* server, connection_t *conn, Arena *arena, struct epoll_event* event);

static inline void print_event(struct epoll_event * event){
    DEBUG_TCP_SERVER("Event:\n");
    DEBUG_TCP_SERVER("  Data: ptr = %p\n", event->data.ptr);
}

// 初始化和生命周期
tcpServer* tcp_server_create(const char* host, int port){
    tcpServer* server = (tcpServer *)malloc(sizeof(tcpServer));

    server->port = port;
    snprintf(server->host, sizeof(server->host), "%s", host);
    server->running = false;
    server->max_connections = TCP_SERVER_CONNECTION_COUNT;

    return server;
}

void tcp_server_destroy(tcpServer *server){
    if (server->epoll_fd >= 0) close(server->epoll_fd);
    if (server->listen_fd >= 0) close(server->listen_fd);
    free(server);
    return;    
}

/* 启动服务器 返回0表示正确 或者 def.h中的错误码 */
int tcp_server_start(tcpServer* server) {
    server->listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 原代码：bind()
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(server->port);
    
    if (bind(server->listen_fd, (struct sockaddr*)&local, sizeof(local)) < 0){
        tcp_server_destroy(server);
        return ERROR_SYSTEM;
    }

    listen(server->listen_fd, SOMAXCONN);
    server->epoll_fd = epoll_create1(0);

    struct epoll_event temp = {0};
    temp.events = EPOLLIN | EPOLLOUT | EPOLLET;
    epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, server->listen_fd, &temp);
    server->running = true;

    return 0;
}


/* 服务器运行 */
int tcp_server_run(tcpServer* server){
    Arena arena_global = {0}; // 全局内存分配器

    struct epoll_event * event_array = (struct epoll_event *)calloc(TCP_SERVER_MAX_EVENTS, sizeof(struct epoll_event));
    connection_t * conn_array = (connection_t *)calloc(server->max_connections, sizeof(connection_t));

    while (1){
        int event_count = epoll_wait(server->epoll_fd, event_array, TCP_SERVER_MAX_EVENTS, -1);
        DEBUG_TCP_SERVER("%d events coming !\n", event_count);

        for (int i = 0; i < event_count; i ++){
            print_event(&event_array[i]);
            connection_t* conn = event_array[i].data.ptr;
            int fd = conn ? conn->fd : server->listen_fd;

            DEBUG_TCP_SERVER("fd is %d, conn ptr is %p\n", fd, conn);
            if (fd == server->listen_fd){ 
                server_handle_accept_event(server, &event_array[i] ,conn_array, &arena_global, event_array);
            }else{ 
                server_handle_event(server, conn, &arena_global, &event_array[i]);
            }
        }

        if (!server->running) break;
    }

// clean_and_return:
    free(event_array);
    free(conn_array);
    arena_free(&arena_global);
    return 0;
}

/*
* 接受accept请求，并且在epoll中注册新事件，创建新conn连接请求
*/
int server_handle_accept_event(tcpServer* server, struct epoll_event* listen_event, connection_t *conn_array, Arena *arena, struct epoll_event* event_array){
    if (event_check(listen_event, EPOLLERR | EPOLLHUP | EPOLLRDHUP)){
        server->running = false; // 强行关闭
        DEBUG_TCP_SERVER("server_handle_accept_event return error\n");
        return ERROR_SYSTEM;
    }

    struct sockaddr_in cli_addr;
    socklen_t socklen = sizeof(cli_addr);
    
    if (server->stats.current_connections == server->max_connections){
        return ERROR_CONN_FULL;     
    }

    int conn_sock = accept4(server->listen_fd, (struct sockaddr *)&cli_addr, &socklen, SOCK_NONBLOCK);
    
    connection_t *conn = &conn_array[conn_sock];
    struct epoll_event * event = &event_array[conn_sock];

    // TODO : 内存随着连接数量理论上会一直增长，之后再想这个问题
    connection_init(conn, conn_sock, cli_addr, arena);
    
    event->events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP; // 不需要out事件，该事件通常的都是可以写的
    event->data.ptr = conn;
    
    epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, conn_sock, event);
    
    server->stats.current_connections ++;
    server->stats.total_connections ++;

    DEBUG_TCP_SERVER("server_handle_accept_event return sucess\n");
    return 0;
}

int server_handle_event(tcpServer* server, connection_t *conn, Arena *global_arena, struct epoll_event* event){
    int ret = 0;

    if (event_check(event, EPOLLERR)){
        ret = ERROR_SYSTEM; goto clean_and_close;
    }

    if (event_check(event, EPOLLIN)){
        protocolContext *ctx = conn->protocol_ctx;
        
        int n;
        while ((n = connection_recv(conn)) > 0);

        int parse_ret = 0;

        if (!ctx || !(ctx->handler)){
            // 若为首个包 绑定对应的ctx协议处理器 在接下来的流程中一直使用这个协议
            parse_ret = connction_get_protocol_ctx(conn);
            ctx = conn->protocol_ctx; // 注意重新设置新的协议处理器
        }else{
            parse_ret = ctx->handler->on_read(conn);
        }
        
        if (parse_ret < 0){
            if (parse_ret != ERROR_PROTO_NEED_MORE){ // 如果不是需要继续读入的错误直接返回关闭连接
                ret = ERROR_PROTO; 
                goto clean_and_close;
            }
        }else if (parse_ret == 0){ // 正确读入
            int process_ret = ctx->handler->on_process(conn); // 处理
            if (process_ret != 0){
                ret = ERROR_PROTO; goto clean_and_close;
            }
        }
    }

    if (event_check(event, EPOLLOUT)){
        if (connection_send(conn) < 0){
            ret = ERROR_SYSTEM;
            goto clean_and_close;
        }
    }

    if (event_check(event, EPOLLRDHUP | EPOLLHUP)) {
        epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, conn->fd, event);
        tcp_server_close_connection(server, conn); // 正常关闭
    }

    return 0;
clean_and_close:
    tcp_server_close_connection(server, conn);;
    return ret;
}

/* 服务器关闭一个connction_t连接并且reset */
void tcp_server_close_connection(tcpServer* server, connection_t* conn){
    if (!conn) return;
    
    if (conn->protocol_ctx) {
        connection_clear_protocol(conn);
    }
    
    server->stats.current_connections--;
    connection_reset(conn);
}

