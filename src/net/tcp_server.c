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
#include "ds/str.h"

#include "protocol/http.h"

#define DEBUG_TCP_SERVER(level, ...) DEBUG(DEBUG_FLAG_TCPSERVER, level, ##__VA_ARGS__)

tcpServer* tcp_server_create(const char* host, int port);
int tcp_server_start(tcpServer* server);
void tcp_server_destroy(tcpServer* server);
int tcp_server_run(tcpServer* server);
void tcp_server_close_connection(tcpServer* server, Connection *conn);
int server_handle_tcp_event(tcpServer* server, Connection *conn, MemoryArena *arena, struct epoll_event* event);

static inline void tcpserver_delete_tcp_and_timer(EventTcpContext *tcp_event, EventTimerContext *time_event);
static uint8_t temp_array[1]; // 用于不传输信息的读写事件

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
        DEBUG_TCP_SERVER(LV_ERROR, ANSI_RED "[tcpServer] (null)\n" ANSI_RESET);
        return;
    }

    // 表头
    DEBUG_TCP_SERVER(LV_INFO,ANSI_CYAN
           "================= tcpServer =================\n"
           ANSI_RESET);

    // ---- 配置 ----
    DEBUG_TCP_SERVER(LV_INFO, ANSI_MAGENTA "[config]\n" ANSI_RESET);
    DEBUG_TCP_SERVER(LV_INFO, "  " ANSI_YELLOW "host" ANSI_RESET "            : " ANSI_GREEN "%s" ANSI_RESET "\n",
           server->host[0] ? server->host : "(empty)");
    DEBUG_TCP_SERVER(LV_INFO, "  " ANSI_YELLOW "port" ANSI_RESET "            : " ANSI_GREEN "%d" ANSI_RESET "\n",
           server->port);
    DEBUG_TCP_SERVER(LV_INFO, "  " ANSI_YELLOW "max_connections" ANSI_RESET " : " ANSI_GREEN "%" PRIu32 ANSI_RESET "\n",
           server->max_connections);

    // ---- 文件描述符 ----
    DEBUG_TCP_SERVER(LV_INFO, ANSI_MAGENTA "[fds]\n" ANSI_RESET);
    DEBUG_TCP_SERVER(LV_INFO, "  " ANSI_YELLOW "listen_fd" ANSI_RESET "       : " ANSI_GREEN "%d" ANSI_RESET "\n",
           server->listen_fd);
    DEBUG_TCP_SERVER(LV_INFO, "  " ANSI_YELLOW "epoll_fd" ANSI_RESET "        : " ANSI_GREEN "%d" ANSI_RESET "\n",
           server->epoll_fd);

    // ---- 状态 ----
    DEBUG_TCP_SERVER(LV_INFO, ANSI_MAGENTA "[state]\n" ANSI_RESET);
    DEBUG_TCP_SERVER(LV_INFO, "  " ANSI_YELLOW "running" ANSI_RESET "         : %s%s" ANSI_RESET "\n",
           server->running ? ANSI_GREEN : ANSI_RED,
           server->running ? "true" : "false");

    // ---- 统计 ----
    DEBUG_TCP_SERVER(LV_INFO, ANSI_MAGENTA "[stats]\n" ANSI_RESET);
    DEBUG_TCP_SERVER(LV_INFO, "  " ANSI_YELLOW "total_connections" ANSI_RESET "   : " ANSI_GREEN "%" PRIu64 ANSI_RESET "\n",
           server->stats.total_connections);
    DEBUG_TCP_SERVER(LV_INFO, "  " ANSI_YELLOW "current_connections" ANSI_RESET " : " ANSI_GREEN "%" PRIu64 ANSI_RESET "\n",
           server->stats.current_connections);

    DEBUG_TCP_SERVER(LV_INFO, "  " ANSI_YELLOW "timer_id_list cost number " ANSI_RESET " : " ANSI_GREEN "%" PRIu64 ANSI_RESET "\n",
           server->timer_id_list->count);

    DEBUG_TCP_SERVER(LV_INFO, "  " ANSI_YELLOW "tcp_id_list cost number " ANSI_RESET " : " ANSI_GREEN "%" PRIu64 ANSI_RESET "\n",
           server->tcp_id_list->count);

    // ---- 内部对象指针 ----
    // 表尾
    DEBUG_TCP_SERVER(LV_INFO, ANSI_CYAN
           "=============================================\n"
           ANSI_RESET);
}

// true 表示需要关闭这个 conn, false 表示不需要关闭这个连接
// 当执行完 tcpserver_on_xxx 事件时调用这个函数判断是否需要关闭 connection
static inline bool is_close_connection(tcpServer * server, Connection* conn){
    DEBUG_TCP_SERVER(LV_INFO, "conn is_keep_alive is %d\n", conn->is_keep_alive);
    if (unlikely(server->stats.current_connections >= ( ( server->max_connections / 10 ) * 9 )) ){ // 服务器资源紧张
        DEBUG_TCP_SERVER(LV_WARN, "conn is almost full, no more resource\n");
        return true;
    }else{
        if (conn->conn_st == CONN_ST_CLOSING) return true;
        if (conn->is_keep_alive){
            // 如果是第1次宽容一点
            if (conn->request_count <= 2){
                if (conn->conn_st == CONN_ST_IDLE) return true; // 闲散关闭
                return false;
            }
            return false; 
        }
    }
    return true;
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
    tcpserver_close_http();
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
    string_set_arena(server->mem_arena);

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

    DEBUG_TCP_SERVER(LV_INFO, "event loop start\n");

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

    if (unlikely( connection_create(conn, conn_fd, local, server->mem_arena) < 0 )){
        DEBUG_TCP_SERVER(LV_WARN, "create connection failed, mem full\n");
        return -1;
    }

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
    EventTcpContext *tcp_ctx,
    int time_fd
){
    struct epoll_event ev = {0};

    memset(time_ctx, 0, sizeof(*time_ctx));
    time_ctx->server = server;
    time_ctx->tcp_event = (void *)tcp_ctx;
    time_ctx->out_time = TCP_SERVRE_DEFAULT_OUT_TIME;

    time_ctx->e.e_type = EVENT_TYPE_TIMER;
    time_ctx->e.e_fd = time_fd;
    time_ctx->e.on_error = tcpserver_time_on_error;
    time_ctx->e.on_read = tcpserver_time_on_read;

    if (unlikely(event_loop_add(server->epoll_fd, (void *)time_ctx, &ev) < 0)) {
        return -1;
    }

    return 0;
}

void tcpserver_listen_on_read(void *temp_ctx){
    DEBUG_TCP_SERVER(LV_INFO, "listen on read start\n");
    EventListenContext * ctx = temp_ctx;
    tcpServer * server = ctx->server;

    if (unlikely(server->running == false)) {
        return;
    }

    int new_idx = -1;
    int new_idx2 = -1;

    int conn_fd = -1;
    int time_fd = -1;

    while (1){
        if (unlikely(server->stats.current_connections == server->max_connections) ){
            DEBUG_TCP_SERVER(LV_WARN, "server current connection full, refuse tcp\n");
            goto clean_idx;
        }

        new_idx = -1;
        new_idx2 = -1;
        time_fd = -1;
        conn_fd = -1;

        new_idx = id_list_get(server->tcp_id_list);
        new_idx2 = id_list_get(server->timer_id_list);

        if (unlikely(new_idx < 0 || new_idx2 < 0)){
            DEBUG_TCP_SERVER(LV_WARN, "no more idx\n");
            goto clean_idx;
        }

        struct sockaddr_in local;
        conn_fd = tcpserver_accept(server, &local);

        if (conn_fd < 0){
            DEBUG_TCP_SERVER(LV_INFO, "connection fd create failed\n");
            if (likely(errno == EAGAIN)){
                DEBUG_TCP_SERVER(LV_INFO, "EAGAIN error, normal return, server is OK \n");
            }else{
                DEBUG_TCP_SERVER(LV_WARN, "server fd is full, refuse tcp\n");
            }
            goto clean_fd;
        }

        time_fd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (time_fd < 0){
            DEBUG_TCP_SERVER(LV_WARN, "time fd create failed\n");
            DEBUG_TCP_SERVER(LV_WARN, "time fd is full, refuse tcp\n");
            goto clean_fd;
        }

        // tcp_ctx init
        DEBUG_TCP_SERVER(LV_INFO, "successent create now ...\n");
        EventTcpContext * tcp_ctx = &(server->event_tcp_array[new_idx]);
        EventTimerContext * time_ctx = &(server->event_timer_array[new_idx2]);
        tcp_ctx->conn = &(server->conn_array[new_idx]);

        int temp_ret = tcpserver_add_tcp_event(server, tcp_ctx, conn_fd, &local, time_ctx);
        if (unlikely(temp_ret < 0)){
            // 非严重错误跳success
            if (temp_ret == ERROR_MEM_FULL){
                goto clean_fd;
            }else{
                goto clean_and_return;
            }
        }
        DEBUG_TCP_SERVER(LV_INFO, "tcp event create success\n");

        // timer_ctx init
        DEBUG_TCP_SERVER(LV_INFO, "timer event create now ...\n");
        if (unlikely(tcpserver_add_timer_event(server, time_ctx, tcp_ctx, time_fd) < 0)){
            goto clean_and_return;
        }
        DEBUG_TCP_SERVER(LV_INFO, "timer event create success\n");
        server->stats.current_connections ++;
        server->stats.total_connections ++;
    }

    return;

clean_fd:
    if (conn_fd > 0) close(conn_fd);
    if (time_fd > 0) close(time_fd);
    DEBUG_TCP_SERVER(LV_WARN, "clean_fd\n");
clean_idx:
    // TODO : 也许可以处理更复杂的情况，这里直接简单处理即可
    if (new_idx >= 0) id_list_add(server->tcp_id_list, new_idx);
    if (new_idx2 >= 0) id_list_add(server->timer_id_list, new_idx2);
    DEBUG_TCP_SERVER(LV_WARN, "clean idx\n");
    return;

clean_and_return: // 严重错误直接终止服务器运行
    ctx->e.error_reason = errno;
    ctx->e.on_error(temp_ctx);
}

void tcpserver_listen_on_error(void *temp_ctx){
    EventListenContext * ctx = temp_ctx;
    tcpServer * server = ctx->server;

    DEBUG_TCP_SERVER(LV_WARN, "server error because %d\n", ctx->e.error_reason);
    
    if (unlikely(server->running == false)) {
        return;
    }
    
    server->running = false;
}

// tcpserver中的tcp事件触发读入
void tcpserver_tcp_on_read(void * temp_ctx){
    DEBUG_TCP_SERVER(LV_INFO, "tcp on read start\n");
    EventTcpContext *ctx = temp_ctx;
    Connection * conn = ctx->conn;
    int parse_ret = 0;

    int conn_ret = connection_recv(conn);

    if (unlikely(conn_ret != EAGAIN && conn_ret != 0)){
        DEBUG_TCP_SERVER(LV_ERROR, "connection_recv return a error %d\n", conn_ret);
        ctx->e.error_reason = conn_ret;
        goto clean_and_close;
    }
    
    // WARN: 只处理http协议，不支持其他协议
    protocolHandler *handler = conn->protocol_handler;
    
    // 读入 HTTP 报文
    parse_ret = handler->on_read(conn);

    if (unlikely( parse_ret < 0 )){
        ctx->e.error_reason = parse_ret;
        if (parse_ret != ERROR_HTTP_NEED_MORE){ // 如果不是需要继续读入的错误直接返回关闭连接
            DEBUG_TCP_SERVER(LV_WARN, "http: prtocol error\n");
            goto clean_and_close;
        }else{ //http 没有受到完整数据报文
            DEBUG_TCP_SERVER(LV_WARN, "http: need more data\n");
        }
        return;
    }

    DEBUG_TCP_SERVER(LV_INFO, "connection-http read success\n");

    // 调用 on_process 处理 http 协议
    int proto_ret = handler->on_process(conn); 
    if (proto_ret < 0){
        DEBUG_TCP_SERVER(LV_WARN, "http return < 0\n");
        ctx->e.error_reason = proto_ret;
        goto clean_and_close;
    }

    switch (proto_ret) {
        case TCP_PROTO_SEND: // 专向 on_write 事件处理
            DEBUG_TCP_SERVER(LV_INFO, "http return send data\n");
            ASSERT(
                event_loop_mod(
                    ((tcpServer *)(ctx->server))->epoll_fd, 
                    conn->fd, 
                    EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP, 
                    temp_ctx
                ) >= 0
            );
            break;
        case TCP_PROTO_CLOSE: // http 要求关闭则直接关闭
            ASSERT(handler->on_close(conn) >= 0);
            ctx->e.error_reason = 0;
            DEBUG_TCP_SERVER(LV_INFO, "http return close\n");
            goto clean_and_close;
        default:
            ASSERT(0);
    }
    
    if ( unlikely(is_close_connection(ctx->server, conn)) ){
        DEBUG_TCP_SERVER(LV_WARN, "tcp server choose to close this connection\n");
        goto clean_and_close;
    }
    DEBUG_TCP_SERVER(LV_INFO, "connection-http process success\n");

    return;
clean_and_close:
    ctx->e.on_error(temp_ctx);
}

void tcpserver_delete_tcp_and_timer(EventTcpContext *tcp_event, EventTimerContext *time_event){
    DEBUG_TCP_SERVER(LV_INFO, "tcpserver_delete_tcp_and_timer\n");
    ASSERT(tcp_event && time_event); // 两者一般同时存在
    tcpServer *server = (tcpServer *)tcp_event->server;
    
    if (tcp_event){
        event_loop_del(server->epoll_fd, tcp_event->e.e_fd);
        tcp_server_close_connection(server, tcp_event->conn);
        int tcp_idx = tcp_event - server->event_tcp_array;
        id_list_add(server->tcp_id_list, tcp_idx);
    }

    if (time_event){
        event_loop_del(server->epoll_fd, time_event->e.e_fd);
        close(time_event->e.e_fd);
        int timer_idx = time_event - server->event_timer_array;
        id_list_add(server->timer_id_list, timer_idx);
    }

    time_event->tcp_event = NULL;
    tcp_event->time_event = NULL;
}

void tcpserver_tcp_on_error(void *temp_ctx){
    DEBUG_TCP_SERVER(LV_INFO, "delete a tcp event\n");

    EventTcpContext *ctx = temp_ctx;
    EventTimerContext *time_ctx = (EventTimerContext *)ctx->time_event;

    if ( unlikely(ctx->e.error_reason < 0) ) {
        DEBUG_TCP_SERVER(LV_WARN, "tcp event error because %d\n", ctx->e.error_reason);
    }

    tcpserver_delete_tcp_and_timer(ctx, time_ctx);
}

void tcpserver_time_on_error(void *temp_ctx){
    DEBUG_TCP_SERVER(LV_INFO, "delelte a time event\n");

    EventTimerContext *ctx = temp_ctx;
    EventTcpContext *tcp_ctx = ctx->tcp_event;

    if (ctx->e.error_reason != 0) {
        DEBUG_TCP_SERVER(LV_WARN, "time event error because %d\n", ctx->e.error_reason);
    }

    tcpserver_delete_tcp_and_timer(tcp_ctx, ctx);
}

void tcpserver_tcp_on_write(void * temp_ctx){
    EventTcpContext *ctx = temp_ctx;
    Connection *conn = ctx->conn;
    
    protocolHandler* handler = conn->protocol_handler;
    httpResponse* res = &((HttpContext *)( conn->protocol_ctx))->http_res;
    handler->on_write(conn);

    switch (res->send_st) {
        case HTTP_SEND_DONE : 
            // 传输完毕
            ASSERT(
                event_loop_mod(
                    ((tcpServer *)(ctx->server))->epoll_fd, 
                    conn->fd, 
                    EPOLLIN | EPOLLET | EPOLLRDHUP, 
                    temp_ctx
                ) >= 0
            ); 
            buffer_clean(conn->send_buf);
            buffer_clean(conn->read_buf);
        case HTTP_SEND_WAIT: case HTTP_SEND_BODY: case HTTP_SEND_HEAD:
            break;
        case  HTTP_SEND_ERROR :
            ctx->e.error_reason = ERROR_SYSTEM;
            goto clean_and_return;
    }

    if ( unlikely(is_close_connection(ctx->server, conn)) ){
        DEBUG_TCP_SERVER(LV_WARN, "tcp server choose to close conn\n");
        goto clean_and_return;
    }
    return;
clean_and_return:
    ctx->e.on_error(temp_ctx);
}

// timer_event 读取时钟并且查看是否超时，如果超时报告给 on_error 事件处理
// 通过 on_error 间接完成 删除 timer_event 和 其对应的 tcp_event
void tcpserver_time_on_read(void *temp_ctx){
    DEBUG_TCP_SERVER(LV_INFO, "tcpserver_time_on_read start\n");
    EventTimerContext* ctx = temp_ctx;

    uint64_t now = global_get_time();
    Connection* conn = (ctx->tcp_event->conn);

    read(ctx->e.e_fd, &temp_array, sizeof(temp_array)); // 随便读一下保持时间循环正确

    if (unlikely( (conn->conn_st == CONN_ST_CLOSING) && (now - conn->last_activity > ctx->out_time) )){
        DEBUG_TCP_SERVER(LV_WARN, "connection close for : time out\n");
        
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

    write(conn->fd, temp_array, sizeof(temp_array)); // last ack

    close(conn->fd);
    server->stats.current_connections--;
    connection_close(conn);
}

