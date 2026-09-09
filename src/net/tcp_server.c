#define _GNU_SOURCE
#include "ds/buffer.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>   

#include "net/connection.h"
#include "net/tcp_server.h"
#include "net/event.h"

#include "other/def.h"
#include "other/debug.h"

#include "protocol/protocol.h"


#define DEBUG_TCP_SERVER(...) DEBUG(DEBUG_FLAG_TCPSERVER, ##__VA_ARGS__)

#define TCP_SERVER_CONNECTION_COUNT 512
#define TCP_SERVER_MAX_EVENTS 2048

#define event_check(e, flag) (e)->events & (flag)

tcpServer* tcp_server_create(const char* host, int port);
int tcp_server_start(tcpServer* server);
void tcp_server_destroy(tcpServer* server);
int tcp_server_run(tcpServer* server);
void tcp_server_close_connection(tcpServer* server, Connection *conn);

// new function for event

int server_handle_tcp_event(tcpServer* server, Connection *conn, MemoryArena *arena, struct epoll_event* event);

static inline void print_event(struct epoll_event * event){
    DEBUG_TCP_SERVER("Event:\n");
    DEBUG_TCP_SERVER("  Data: ptr = %p\n", event->data.ptr);
}

/* 
 * tcp服务器接收一个服务并且返回一个 conn_sock ，该 conn_sock 为非阻塞的
 */
static inline int tcpserver_accept(tcpServer *server, struct sockaddr_in * cli_addr){
    socklen_t socklen = sizeof(*cli_addr);
    
    // 注意设置这个为 SOCK_NONBLOCK 非阻塞监听，要求 while 1 : read or send data 
    int conn_sock = accept4(server->listen_fd, (struct sockaddr *)cli_addr, &socklen, SOCK_NONBLOCK);
    return conn_sock;
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
    temp.events = EPOLLIN;
    epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, server->listen_fd, &temp);
    server->running = true;

    return 0;
}


/* 服务器运行 */
int tcp_server_run(tcpServer* server){
    server->conn_array = (Connection *)malloc(sizeof(Connection) * TCP_SERVER_CONNECTION_COUNT);
    server->event_tcp_array = (EventTcpContext* )malloc(sizeof(EventTcpContext) * TCP_SERVER_MAX_EVENTS);
    server->event_timer_array = (EventTimerContext* )malloc(sizeof(EventTimerContext) * TCP_SERVER_MAX_EVENTS);

    while (1){
        if (event_loop_run(server->epoll_fd, -1) < 0) server->running = false;
        if (!server->running) break;
    }

    free(server->conn_array);
    free(server->event_tcp_array);
    free(server->event_timer_array);
}


void tcpserver_listen_on_read(void *temp_ctx){
    EventListenContext * ctx = temp_ctx;
    tcpServer * server = ctx->server;

    if (IS_ERR(server->running)) {
        return;
    }

    if (unlikely(server->stats.current_connections == server->max_connections) ){
        return;     
    }

    // TODO : 如何分配一个ctx给add使用 和一个ev给add使用
    // 需要使用bitmap类似的能力找到可以分配的空闲Conection 和 event 

    EventTcpContext * tcp_ctx = NULL;
    struct epoll_event* ev = NULL;
    event_loop_add(server->epoll_fd, (void *)tcp_ctx, ev);

    server->stats.current_connections ++;
    server->stats.total_connections ++;
}

void tcpserver_listen_on_error(void *temp_ctx){
    EventListenContext * ctx = temp_ctx;
    tcpServer * server = ctx->server;

    if (IS_ERR(server->running)) {
        return;
    }
    
    server->running = false;
}

void tcpserver_tcp_on_read(void * temp_ctx){
    EventTcpContext *ctx = temp_ctx;
    Connection * conn = ctx->conn;
    
    protocolHandler *handler = conn->protocol_handler;

    int n;
    while ((n = connection_recv(conn)) > 0);
    int parse_ret = 0;

    if (!(handler)){
        // 若为首个http 绑定对应的handler协议处理器 在接下来的流程中一直使用这个协议
        DEBUG_TCP_SERVER("绑定对应protocol context\n");
        parse_ret = connection_get_protocol_ctx(conn); 
        handler = conn->protocol_handler; // 注意重新设置新的协议处理器
    }else{
        DEBUG_TCP_SERVER("已经来过一次，不绑定protocol context\n");
        parse_ret = handler->on_read(conn);
    }

    if (parse_ret < 0){
        if (parse_ret != ERROR_PROTO_NEED_MORE){ // 如果不是需要继续读入的错误直接返回关闭连接
            DEBUG_TCP_SERVER("server_handle_tcp_event: prtocol error\n");
            goto clean_and_close;
        }
    }else if (parse_ret == 0){ // 正确读入
        DEBUG_TCP_SERVER("server_handle_tcp_event protocol read sucess\n");
        int process_ret = handler->on_process(conn); // 处理 注意：该接口是唯一和http接触的接口
        if (process_ret != 0){
            goto clean_and_close;
        }
        DEBUG_TCP_SERVER("server_handle_tcp_event: protocol process sucess\n");
    }

    return;
clean_and_close:
    tcp_server_close_connection(ctx->server, conn);
}

void tcpserver_tcp_on_error(void * temp_ctx){
    EventTcpContext *ctx = temp_ctx;
    tcp_server_close_connection(ctx->server, ctx->conn);;
}

void tcpserver_tcp_on_write(void * temp_ctx){
    EventTcpContext *ctx = temp_ctx;

    Connection *conn = ctx->conn;
    if (connection_send(conn, ctx->buffer) < 0){
        DEBUG_TCP_SERVER("server_handle_tcp_event: system error\n");
        tcp_server_close_connection(ctx->server, ctx->conn);;
   }
}

/* 服务器关闭一个connction_t连接并且reset */
void tcp_server_close_connection(tcpServer* server, Connection* conn){
    if (!conn) return;
    
    connection_clear_protocol(conn);
    
    server->stats.current_connections--;
    connection_close(conn);
}

