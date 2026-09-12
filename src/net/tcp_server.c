#define _GNU_SOURCE
#include <asm-generic/errno-base.h>
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
#define TCP_SERVRE_DEFAULT_OUT_TIME 500

#define event_check(e, flag) (e)->events & (flag)

tcpServer* tcp_server_create(const char* host, int port);
int tcp_server_start(tcpServer* server);
void tcp_server_destroy(tcpServer* server);
int tcp_server_run(tcpServer* server);
void tcp_server_close_connection(tcpServer* server, Connection *conn);
int server_handle_tcp_event(tcpServer* server, Connection *conn, MemoryArena *arena, struct epoll_event* event);

static inline void tcpserver_delete_tcp_and_timer(EventTcpContext *tcp_event, EventTimerContext *time_event);
static uint8_t temp_array[16];

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

    DEBUG_TCP_SERVER("  " ANSI_YELLOW "timer_id_list cost number " ANSI_RESET " : " ANSI_GREEN "%" PRIu64 ANSI_RESET "\n",
           server->timer_id_list->count);

    DEBUG_TCP_SERVER("  " ANSI_YELLOW "tcp_id_list cost number " ANSI_RESET " : " ANSI_GREEN "%" PRIu64 ANSI_RESET "\n",
           server->tcp_id_list->count);

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

static inline int tcpserver_add_tcp_event(
    tcpServer *server,
    EventTcpContext *tcp_ctx,
    int conn_fd,
    struct sockaddr_in *local,
    EventTimerContext* time_ctx
){
    Connection *conn = tcp_ctx->conn;
    struct epoll_event ev = {0};

    connection_create(conn, conn_fd, local, server->mem_arena);

    memset(tcp_ctx, 0, sizeof(*tcp_ctx));
    tcp_ctx->conn = conn;
    tcp_ctx->global_arena = server->mem_arena;
    tcp_ctx->server = server;
    tcp_ctx->time_event = (void *)time_ctx;

    tcp_ctx->e.e_fd = conn_fd;
    tcp_ctx->e.e_type = EVENT_TYPE_TCP;
    tcp_ctx->e.on_error = tcpserver_tcp_on_error;
    tcp_ctx->e.on_read = tcpserver_tcp_on_read;
    tcp_ctx->e.on_write = tcpserver_tcp_on_write;

    if (unlikely(event_loop_add(server->epoll_fd, (void *)tcp_ctx, &ev) < 0)) {
        return -1;
    }

    return 0;
}

static inline int tcpserver_add_timer_event(
    tcpServer *server,
    EventTimerContext *time_ctx,
    EventTcpContext *tcp_ctx)
{
    struct epoll_event ev = {0};
    int time_fd = timerfd_create(CLOCK_MONOTONIC, 0);

    if (unlikely(time_fd < 0)) {
        return -1;
    }

    memset(time_ctx, 0, sizeof(*time_ctx));
    time_ctx->server = server;
    time_ctx->tcp_event = (void *)tcp_ctx;
    time_ctx->out_time = TCP_SERVRE_DEFAULT_OUT_TIME;

    time_ctx->e.e_type = EVENT_TYPE_TIMER;
    time_ctx->e.e_fd = time_fd;
    time_ctx->e.on_error = tcpserver_time_on_error;
    time_ctx->e.on_read = tcpserver_time_on_read;

    if (unlikely(event_loop_add(server->epoll_fd, (void *)time_ctx, &ev) < 0)) {
        close(time_fd);
        return -1;
    }

    return 0;
}

void tcpserver_listen_on_read(void *temp_ctx){
    DEBUG_TCP_SERVER("listen on read start\n");
    EventListenContext * ctx = temp_ctx;
    tcpServer * server = ctx->server;

    if (unlikely(server->running == false)) {
        return;
    }

    int new_idx = -1;
    int new_idx2 = -1;

    while (1){
        if (unlikely(server->stats.current_connections == server->max_connections) ){
            DEBUG_TCP_SERVER("server current connection full, refuse tcp\n");
            goto no_more_resource;
        }

        new_idx = id_list_get(server->tcp_id_list);
        new_idx2 = id_list_get(server->timer_id_list);

        if (unlikely(new_idx < 0 || new_idx2 < 0)){
            DEBUG_TCP_SERVER("no more idx\n");
            goto no_more_resource;
        }

        struct sockaddr_in local;
        int conn_fd = tcpserver_accept(server, &local);
        DEBUG_TCP_SERVER("accept a new connection\n");

        if (conn_fd < 0){
            if (unlikely(errno != EAGAIN)){
                ctx->e.error_reason = errno;
                ctx->e.on_error(temp_ctx);
            }else{
                DEBUG_TCP_SERVER("EAGAIN error, i do not care\n");
            }
            goto no_more_resource;
        }

        // tcp_ctx init
        DEBUG_TCP_SERVER("tcp event create now ...\n");
        EventTcpContext * tcp_ctx = &(server->event_tcp_array[new_idx]);
        EventTimerContext * time_ctx = &(server->event_timer_array[new_idx2]);

        tcp_ctx->conn = &(server->conn_array[new_idx]);

        if (unlikely(tcpserver_add_tcp_event(server, tcp_ctx, conn_fd, &local, time_ctx) < 0)){
            goto clean_and_return;
        }
        DEBUG_TCP_SERVER("tcp event create sucess\n");

        // timer_ctx init
        DEBUG_TCP_SERVER("timer event create now ...\n");

        if (unlikely(tcpserver_add_timer_event(server, time_ctx, tcp_ctx) < 0)){
            server->running = false;
            goto clean_and_return;
        }
        DEBUG_TCP_SERVER("timer event create sucess\n");

        server->stats.current_connections ++;
        server->stats.total_connections ++;

        new_idx = -1;
        new_idx2 = -1;
    }

    return;

no_more_resource:
    // TODO : 也许可以处理更复杂的情况，直接跳过接受 Connection 有点丑陋了

    if (new_idx >= 0) id_list_add(server->tcp_id_list, new_idx);
    if (new_idx2 >= 0) id_list_add(server->timer_id_list, new_idx2);
    DEBUG_TCP_SERVER("tcp server say: no_more_resource\n");
    return;

clean_and_return: // 严重错误直接终止服务器运行
    server->running = false;
    ctx->e.error_reason = errno;
    ctx->e.on_error(temp_ctx);
}

void tcpserver_listen_on_error(void *temp_ctx){
    EventListenContext * ctx = temp_ctx;
    tcpServer * server = ctx->server;

    DEBUG_TCP_SERVER("server error because %d\n", ctx->e.error_reason);
    
    if (unlikely(server->running == false)) {
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

void tcpserver_delete_tcp_and_timer(EventTcpContext *tcp_event, EventTimerContext *time_event){
    DEBUG_TCP_SERVER("tcpserver_delete_tcp_and_timer\n");
    ASSERT(tcp_event && time_event); // 两者一般同时存在
    tcpServer *server = (tcpServer *)tcp_event->server;
    
    {
        // 从 epoll 中移除 TCP 事件
        event_loop_del(server->epoll_fd, tcp_event->e.e_fd);

        // 关闭连接（内部会关闭 fd，并减少 current_connections）
        tcp_server_close_connection(server, tcp_event->conn);
        tcp_event->conn = NULL; // 标记已删除

        // 归还 TCP id
        int tcp_idx = tcp_event - server->event_tcp_array;
        id_list_add(server->tcp_id_list, tcp_idx);
    }

    {
        // 从 epoll 中移除 timer 事件
        event_loop_del(server->epoll_fd, time_event->e.e_fd);
        close(time_event->e.e_fd);
        time_event->e.e_fd = -1;

        time_event->tcp_event = NULL; // 标记已删除
        // 归还 timer id
        int timer_idx = time_event - server->event_timer_array;
        id_list_add(server->timer_id_list, timer_idx);
    }

    time_event->tcp_event = NULL;
    tcp_event->time_event = NULL;
}

void tcpserver_tcp_on_error(void *temp_ctx){
    DEBUG_TCP_SERVER("delete a tcp event\n");

    EventTcpContext *ctx = temp_ctx;
    EventTimerContext *time_ctx = (EventTimerContext *)ctx->time_event;

    if (ctx->e.error_reason != 0) {
        DEBUG_TCP_SERVER("tcp event error because %d\n", ctx->e.error_reason);
    }

    tcpserver_delete_tcp_and_timer(ctx, time_ctx);
}

void tcpserver_time_on_error(void *temp_ctx){
    DEBUG_TCP_SERVER("delelte a time event\n");

    EventTimerContext *ctx = temp_ctx;
    EventTcpContext *tcp_ctx = ctx->tcp_event;

    if (ctx->e.error_reason != 0) {
        DEBUG_TCP_SERVER("time event error because %d\n", ctx->e.error_reason);
    }

    tcpserver_delete_tcp_and_timer(tcp_ctx, ctx);
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

// timer_event 读取时钟并且查看是否超时，如果超时报告给 on_error 事件处理
// 通过 on_error 间接完成 删除 timer_event 和 其对应的 tcp_event
void tcpserver_time_on_read(void *temp_ctx){
    DEBUG_TCP_SERVER("delete a timer event\n");
    EventTimerContext* ctx = temp_ctx;

    uint64_t now = global_get_time();
    Connection* conn = (ctx->tcp_event->conn);

    read(ctx->e.e_fd, &temp_array, sizeof(temp_array)); // 随便读一下保持时间循环正确

    if (unlikely( (conn->is_dead == false) && (now - conn->last_activity > ctx->out_time) )){
        DEBUG_TCP_SERVER("connection close for : time out\n");
        
        // 报告两个错误哦
        ctx->tcp_event->e.error_reason = ERROR_TCP_TIME_OUT;
        ctx->e.error_reason = ERROR_TCP_TIME_OUT;

        // 会将两个同时删除
        ctx->e.on_error(temp_ctx);
    }
}

/* 服务器关闭一个connction_t连接并且reset */
void tcp_server_close_connection(tcpServer* server, Connection* conn){
    ASSERT(conn);
    connection_clear_protocol(conn);
    close(conn->fd);
    server->stats.current_connections--;
    connection_close(conn);
}

