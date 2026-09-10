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

#include "other/global_time.h"
#include "other/def.h"
#include "other/debug.h"

#include "protocol/protocol.h"


#define DEBUG_TCP_SERVER(...) DEBUG(DEBUG_FLAG_TCPSERVER, ##__VA_ARGS__)

#define TCP_SERVER_CONNECTION_COUNT 2048

#define event_check(e, flag) (e)->events & (flag)

tcpServer* tcp_server_create(const char* host, int port);
int tcp_server_start(tcpServer* server);
void tcp_server_destroy(tcpServer* server);
int tcp_server_run(tcpServer* server);
void tcp_server_close_connection(tcpServer* server, Connection *conn);

// new function for event

int server_handle_tcp_event(tcpServer* server, Connection *conn, MemoryArena *arena, struct epoll_event* event);

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
    server->mem_arena = arena_create(0);
    server->conn_array = (Connection *)malloc(sizeof(Connection) * TCP_SERVER_CONNECTION_COUNT);
    server->event_tcp_array = (EventTcpContext* )malloc(sizeof(EventTcpContext) * TCP_SERVER_CONNECTION_COUNT);
    server->event_timer_array = (EventTimerContext* )malloc(sizeof(EventTimerContext) * TCP_SERVER_CONNECTION_COUNT);
    server->timer_id_list = id_list_create(TCP_SERVER_CONNECTION_COUNT);
    server->tcp_id_list = id_list_create(TCP_SERVER_CONNECTION_COUNT);

    while (1){
        if (event_loop_run(server->epoll_fd, -1) < 0) server->running = false;
        if (!server->running) break;
    }

    free(server->conn_array);
    free(server->event_tcp_array);
    free(server->event_timer_array);
    id_list_free(server->tcp_id_list);
    id_list_free(server->timer_id_list);
    arena_free(server->mem_arena);

    return 0;
}


void tcpserver_listen_on_read(void *temp_ctx){
    EventListenContext * ctx = temp_ctx;
    tcpServer * server = ctx->server;

    if (IS_ERR(server->running)) {
        return;
    }

    if (unlikely(server->stats.current_connections == server->max_connections) ){
        DEBUG_TCP_SERVER("server connection full\n");
        return;     
    }

    int new_idx = id_list_get(server->tcp_id_list);

    EventTcpContext * tcp_ctx = &(server->event_tcp_array[new_idx]);
    Connection * conn = &(server->conn_array[new_idx]);
    struct epoll_event ev = {0};
    
    struct sockaddr_in local;
    int conn_fd = tcpserver_accept(server, &local);
    if (IS_ERR(bind(server->listen_fd, (struct sockaddr*)&local, sizeof(local)))){
        server->running = false;
        return;
    }
    connection_create(conn, conn_fd, &local, server->mem_arena);

    // tcp_ctx init
    tcp_ctx->conn = conn;
    tcp_ctx->global_arena = server->mem_arena;
    tcp_ctx->server = server;

    tcp_ctx->e.e_type = EVENT_TYPE_TCP;
    tcp_ctx->e.on_error = tcpserver_tcp_on_error;
    tcp_ctx->e.on_read = tcpserver_tcp_on_read;
    tcp_ctx->e.on_write = tcpserver_tcp_on_write;

    if (IS_ERR(event_loop_add(server->epoll_fd, (void *)tcp_ctx, &ev))){
        server->running = false;
        return;
    }
    
    // timer_ctx init
    // TODO : complete timer callback function and register
    EventTimerContext* timer_ctx = {0}; 
    


    server->stats.current_connections ++;
    server->stats.total_connections ++;
}

void tcpserver_listen_on_error(void *temp_ctx){
    EventListenContext * ctx = temp_ctx;
    tcpServer * server = ctx->server;

    DEBUG_TCP_SERVER("server error because %d", ctx->e.error_reason);
    debug_no_no();
    
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

    DEBUG_TCP_SERVER("server error because %d", ctx->e.error_reason);
    debug_no_no();

    tcp_server_close_connection(ctx->server, ctx->conn);;
}

void tcpserver_tcp_on_write(void * temp_ctx){
    EventTcpContext *ctx = temp_ctx;

    Connection *conn = ctx->conn;
    if (connection_send(conn, ctx->conn->send_buf) < 0){
        DEBUG_TCP_SERVER("server_handle_tcp_event: system error\n");
        tcp_server_close_connection(ctx->server, ctx->conn);;
   }
}

void tcpserver_time_on_read(void *temp_ctx){
    EventTimerContext* ctx = temp_ctx;

    uint64_t now = global_get_time();
    
    if (likely( (ctx->conn->is_dead == false) && (now - ctx->conn->last_activity > ctx->out_time) )){
        tcp_server_close_connection((void *)(ctx->server), ctx->conn);
    }
}
/* 服务器关闭一个connction_t连接并且reset */
void tcp_server_close_connection(tcpServer* server, Connection* conn){
    if (!conn) return;
    
    connection_clear_protocol(conn);
    
    server->stats.current_connections--;
    connection_close(conn);
}

