#include <asm-generic/errno-base.h>
#define _GNU_SOURCE
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>   
#include <errno.h>
#include <inttypes.h>

#include "net/connection.h"
#include "net/tcp_server.h"
#include "net/event.h"

#include "other/global_time.h"
#include "other/def.h"
#include "other/debug.h"

#include "protocol/protocol.h"

#include "ds/buffer.h"

#define DEBUG_TCP_SERVER(...) DEBUG(DEBUG_FLAG_TCPSERVER, ##__VA_ARGS__)

#define TCP_SERVER_CONNECTION_COUNT 2048
#define TCP_SERVRE_DEFAULT_OUT_TIME 3

#define event_check(e, flag) (e)->events & (flag)

tcpServer* tcp_server_create(const char* host, int port);
int tcp_server_start(tcpServer* server);
void tcp_server_destroy(tcpServer* server);
int tcp_server_run(tcpServer* server);
void tcp_server_close_connection(tcpServer* server, Connection *conn);

// new function for event

int server_handle_tcp_event(tcpServer* server, Connection *conn, MemoryArena *arena, struct epoll_event* event);

static uint8_t temp_array[64];

/* 
 * tcp服务器接收一个服务并且返回一个 conn_sock ，该 conn_sock 为非阻塞的
 */
static inline int tcpserver_accept(tcpServer *server, struct sockaddr_in * cli_addr){
    socklen_t socklen = sizeof(*cli_addr);
    
    // 注意设置这个为 SOCK_NONBLOCK 非阻塞监听，要求 while 1 : read or send data 
    int conn_sock = accept4(server->listen_fd, (struct sockaddr *)cli_addr, &socklen, SOCK_NONBLOCK);
    return conn_sock;
}

static inline void print_tcpserver(tcpServer* server) {
    if (server == NULL) {
        DEBUG_TCP_SERVER(ANSI_RED "[tcpServer] (null)\n" ANSI_RESET);
        return;
    }

    // 表头
    DEBUG_TCP_SERVER(ANSI_CYAN
           "================= tcpServer =================\n"
           ANSI_RESET);

    // ---- 配置 ----
    DEBUG_TCP_SERVER(ANSI_MAGENTA "[config]\n" ANSI_RESET);
    DEBUG_TCP_SERVER("  " ANSI_YELLOW "host" ANSI_RESET "            : " ANSI_GREEN "%s" ANSI_RESET "\n",
           server->host[0] ? server->host : "(empty)");
    DEBUG_TCP_SERVER("  " ANSI_YELLOW "port" ANSI_RESET "            : " ANSI_GREEN "%d" ANSI_RESET "\n",
           server->port);
    DEBUG_TCP_SERVER("  " ANSI_YELLOW "max_connections" ANSI_RESET " : " ANSI_GREEN "%" PRIu32 ANSI_RESET "\n",
           server->max_connections);

    // ---- 文件描述符 ----
    DEBUG_TCP_SERVER(ANSI_MAGENTA "[fds]\n" ANSI_RESET);
    DEBUG_TCP_SERVER("  " ANSI_YELLOW "listen_fd" ANSI_RESET "       : " ANSI_GREEN "%d" ANSI_RESET "\n",
           server->listen_fd);
    DEBUG_TCP_SERVER("  " ANSI_YELLOW "epoll_fd" ANSI_RESET "        : " ANSI_GREEN "%d" ANSI_RESET "\n",
           server->epoll_fd);

    // ---- 状态 ----
    DEBUG_TCP_SERVER(ANSI_MAGENTA "[state]\n" ANSI_RESET);
    DEBUG_TCP_SERVER("  " ANSI_YELLOW "running" ANSI_RESET "         : %s%s" ANSI_RESET "\n",
           server->running ? ANSI_GREEN : ANSI_RED,
           server->running ? "true" : "false");

    // ---- 统计 ----
    DEBUG_TCP_SERVER(ANSI_MAGENTA "[stats]\n" ANSI_RESET);
    DEBUG_TCP_SERVER("  " ANSI_YELLOW "total_connections" ANSI_RESET "   : " ANSI_GREEN "%" PRIu64 ANSI_RESET "\n",
           server->stats.total_connections);
    DEBUG_TCP_SERVER("  " ANSI_YELLOW "current_connections" ANSI_RESET " : " ANSI_GREEN "%" PRIu64 ANSI_RESET "\n",
           server->stats.current_connections);

    // ---- 内部对象指针 ----

    // 表尾
    DEBUG_TCP_SERVER(ANSI_CYAN
           "=============================================\n"
           ANSI_RESET);
}

// 初始化和生命周期
tcpServer* tcp_server_create(const char* host, int port){
    tcpServer* server = (tcpServer *)malloc(sizeof(tcpServer));

    server->port = port;
    snprintf(server->host, sizeof(server->host), "%s", host);
    server->running = false;
    server->max_connections = TCP_SERVER_CONNECTION_COUNT;
    server->stats.current_connections = 0;
    server->stats.total_connections = 0; 

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

    // add listen event
    EventListenContext ctx = {0};
    struct epoll_event ev = {0};

    ctx.e.e_fd = server->listen_fd;
    ctx.e.e_type = EVENT_TPYE_LISTEN;
    ctx.e.on_read = tcpserver_listen_on_read;
    ctx.e.on_error = tcpserver_listen_on_error;
    ctx.e.on_write = NULL;
    ctx.server = server;

    int temp = event_loop_add(server->epoll_fd, (void *)&ctx, &ev);
    ASSERT(temp >= 0);

    DEBUG_TCP_SERVER("event loop start\n");

    while (1){
        print_tcpserver(server);
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
    DEBUG_TCP_SERVER("listen on read start\n");
    EventListenContext * ctx = temp_ctx;
    tcpServer * server = ctx->server;

    if (unlikely(server->running == false)) {
        return;
    }
    
    while (1){
        if (unlikely(server->stats.current_connections == server->max_connections) ){
            goto no_more_resource;
        }

        int new_idx = id_list_get(server->tcp_id_list);
        int new_idx2 = id_list_get(server->timer_id_list);

        if (unlikely(new_idx < 0 || new_idx2 < 0)){
            DEBUG_TCP_SERVER("no more idx\n");
            goto no_more_resource;
        }

        struct sockaddr_in local;
        int conn_fd = tcpserver_accept(server, &local);
        DEBUG_TCP_SERVER("accept a new connection\n");

        if (conn_fd < 0){
            if (errno == EAGAIN) break;
            else{
                ctx->e.error_reason = errno;
                ctx->e.on_error(temp_ctx);
                return;
            }
        }

        // tcp_ctx init
        DEBUG_TCP_SERVER("tcp event create now ...\n");
        EventTcpContext * tcp_ctx = &(server->event_tcp_array[new_idx]);
        Connection * conn = &(server->conn_array[new_idx]);
        struct epoll_event ev = {0};
        connection_create(conn, conn_fd, &local, server->mem_arena);

        memset(tcp_ctx, 0, sizeof(*tcp_ctx));
        tcp_ctx->conn = conn;
        tcp_ctx->global_arena = server->mem_arena;
        tcp_ctx->server = server;

        tcp_ctx->e.e_type = EVENT_TYPE_TCP;
        tcp_ctx->e.on_error = tcpserver_tcp_on_error;
        tcp_ctx->e.on_read = tcpserver_tcp_on_read;
        tcp_ctx->e.on_write = tcpserver_tcp_on_write;

        if (IS_ERR(event_loop_add(server->epoll_fd, (void *)tcp_ctx, &ev))){
            goto clean_and_return;
        }
        DEBUG_TCP_SERVER("tcp event create sucess\n");
        
        // timer_ctx init
        DEBUG_TCP_SERVER("timer event create now ...\n");
        EventTimerContext* time_ctx = &(server->event_timer_array[new_idx2]);
        int time_fd = timerfd_create(CLOCK_MONOTONIC, 0);   
        memset(time_ctx, 0, sizeof(*time_ctx));

        time_ctx->server = server;
        time_ctx->tcp_event = (void *)(tcp_ctx);
        time_ctx->out_time = TCP_SERVRE_DEFAULT_OUT_TIME;
        time_ctx->e.e_type = EVENT_TYPE_TIMER;
        
        time_ctx->e.e_fd = time_fd;
        time_ctx->e.on_error = tcpserver_time_on_error;
        time_ctx->e.on_read = tcpserver_time_on_read;

        if (IS_ERR(event_loop_add(server->epoll_fd, (void *)time_ctx, &ev))){
            server->running = false;
            goto clean_and_return;
        }
        DEBUG_TCP_SERVER("timer event create sucess\n");

        server->stats.current_connections ++;
        server->stats.total_connections ++;
    }

    return;

no_more_resource:
    // TODO : 也许可以处理更复杂的情况，直接跳过接受 Connection 有点丑陋了
    DEBUG_TCP_SERVER("tcp server say: no_more_resource\n");
    return;
clean_and_return:
    server->running = false;
    ctx->e.error_reason = errno;
    ctx->e.on_error(temp_ctx);
    return;
}

void tcpserver_listen_on_error(void *temp_ctx){
    EventListenContext * ctx = temp_ctx;
    tcpServer * server = ctx->server;

    DEBUG_TCP_SERVER("server error because %d\n", ctx->e.error_reason);
    debug_no_no();
    
    if (IS_ERR(server->running)) {
        return;
    }
    
    server->running = false;
}

void tcpserver_tcp_on_read(void * temp_ctx){
    DEBUG_TCP_SERVER("tcp on read start\n");
    EventTcpContext *ctx = temp_ctx;
    Connection * conn = ctx->conn;
    int parse_ret = 0;
    
    protocolHandler *handler = conn->protocol_handler;

    int n;
    while ((n = connection_recv(conn)) > 0);
    goto clean_and_close;

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
    ctx->e.error_reason = parse_ret;
    ctx->e.on_error(temp_ctx);
}

void tcpserver_tcp_on_error(void * temp_ctx){
    DEBUG_TCP_SERVER("delete a tcp event\n");
    EventTcpContext *ctx = temp_ctx;
    
    if (ctx->e.error_reason != 0){
        DEBUG_TCP_SERVER("server error because %d\n", ctx->e.error_reason);
        debug_no_no();
    }
    
    tcpServer * server = ctx->server;
    tcp_server_close_connection(server, ctx->conn);
    event_loop_del(server->epoll_fd, ctx->e.e_fd); 
    
    int idx = ctx - server->event_tcp_array;
    id_list_add(server->tcp_id_list, idx);
}

void tcpserver_tcp_on_write(void * temp_ctx){
    EventTcpContext *ctx = temp_ctx;

    Connection *conn = ctx->conn;
    int ret = connection_send(conn, ctx->conn->send_buf);

    if (ret < 0){
        DEBUG_TCP_SERVER("server_handle_tcp_event: system error\n");
        ctx->e.error_reason = ret;
        ctx->e.on_error(temp_ctx);
    }
}

void tcpserver_time_on_read(void *temp_ctx){
    DEBUG_TCP_SERVER("delete a timer event\n");
    EventTimerContext* ctx = temp_ctx;

    uint64_t now = global_get_time();
    tcpServer * server = (tcpServer *)(ctx->server);
    Connection* conn = (ctx->tcp_event->conn);

    read(ctx->e.e_fd, &temp_array, sizeof(temp_array)); // 随便读一下保持时间循环正确

    if (unlikely( (conn->is_dead == false) && (now - conn->last_activity > ctx->out_time) )){
        // 调用事件的on_error
        ctx->tcp_event->e.error_reason = ERROR_TCP_TIME_OUT;
        ctx->tcp_event->e.on_error((void *)(ctx->tcp_event));
    }

    event_loop_del(server->epoll_fd, ctx->e.e_fd); 
    close(ctx->e.e_fd);
}

void tcpserver_time_on_error(void *temp_ctx){
    EventTimerContext* ctx = temp_ctx;
    tcpServer * server = (tcpServer *)(ctx->server);
    event_loop_del(server->epoll_fd, ctx->e.e_fd); 

    int idx = ctx - server->event_timer_array;
    id_list_add(server->timer_id_list, idx);
}

/* 服务器关闭一个connction_t连接并且reset */
void tcp_server_close_connection(tcpServer* server, Connection* conn){
    ASSERT(conn);
    connection_clear_protocol(conn);
    
    server->stats.current_connections--;
    connection_close(conn);
}

