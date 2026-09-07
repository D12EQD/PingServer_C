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
    MemoryArena * arena_global = arena_create(0);

    struct epoll_event * epoll_wait_array = (struct epoll_event *)calloc(TCP_SERVER_MAX_EVENTS, sizeof(struct epoll_event));
    struct epoll_event * event_epoll_array = (struct epoll_event *)calloc(TCP_SERVER_MAX_EVENTS, sizeof(struct epoll_event));
    struct epoll_event * event_time_array = (struct epoll_event *)calloc(TCP_SERVER_MAX_EVENTS, sizeof(struct epoll_event));

    Connection * conn_array = (Connection *)calloc(server->max_connections, sizeof(Connection));
    Event * epoll_event_data = (Event *)malloc(sizeof(Event) * TCP_SERVER_MAX_EVENTS);
    Event * time_event_data = (Event *)malloc(sizeof(Event) * TCP_SERVER_MAX_EVENTS);
    
    /*
     * epoll_event_data[fd] conn_array[fd] 可以获得对应事件的真实数据地址
     * epoll_evnet_array 用来接收每次收到的事件数组
     */
    while (1){
        int event_count = epoll_wait(server->epoll_fd, epoll_wait_array, TCP_SERVER_MAX_EVENTS, -1);
        DEBUG_TCP_SERVER("%d events coming !\n", event_count);

        for (int i = 0; i < event_count; i ++){
            print_event(&epoll_wait_array[i]);
            Event * event = epoll_wait_array[i].data.ptr;

            if (event == NULL){
                DEBUG_TCP_SERVER("get a new connection\n");
                struct sockaddr_in cli_addr;
                int new_fd = tcpserver_accept(server, &cli_addr);
                
                Connection * conn = &conn_array[new_fd];
                connection_create(conn, new_fd, &cli_addr, arena_global);
                
                // 处理accept事件并且处理
                server_handle_accept_event(
                    server,
                    arena_global,
                    &epoll_wait_array[i], 
                    new_fd, 
                    conn, 
                    &event_epoll_array[new_fd], 
                    &epoll_event_data[new_fd]
                );

                // 启动一个定时装置
            }else{
                switch (event->e_type){
                    case (EVENT_TYPE_TCP):
                        if (server_handle_tcp_event(server, event->data.tcp_data.conn, arena_global, &epoll_wait_array[i]) < 0){
                            goto clean;
                        }
                        break;
                    case (EVENT_TYPE_TIMER):
                        break;
                    default:
                        ASSERT(0);
                }
            }
            // int fd = conn ? conn->fd : server->listen_fd;
            //
            // DEBUG_TCP_SERVER("fd is %d, conn ptr is %p\n", fd, conn);
            // if (fd == server->listen_fd){ 
            //     server_handle_accept_event(server, &epoll_wait_array[i] ,conn_array, arena_global, epoll_event_data);
            // }else{ 
            //     server_handle_tcp_event(server, conn, arena_global, &epoll_wait_array[i]);
            // }
        }

        if (!server->running) break;
    }


clean:
    free(epoll_wait_array);
    free(conn_array);
    arena_free(arena_global);
    return 0;
}


/*
* 接受accept请求，并且在epoll中注册新事件，创建新conn连接请求，用返回的fd表示数组下标，用于其中的conn_array 和 epoll_event_array
* server : tcp_server 
* event : 传入进入的事件，作为事件的data.ptr使用，保存事件信息
* conn_array : 传入一个 Connection 的数组
*/
int server_handle_accept_event(
    tcpServer* server, 
    MemoryArena *arena, 
    struct epoll_event* listen_event,
    int new_conn_sock_fd,
    Connection *new_conn, 
    struct epoll_event* new_event,
    Event * new_event_data
){
    if (event_check(listen_event, EPOLLERR | EPOLLHUP | EPOLLRDHUP)){
        server->running = false; // 强行关闭
        DEBUG_TCP_SERVER("server_handle_accept_event return error. server close!\n"); 
        return ERROR_SYSTEM;
    }

    if (server->stats.current_connections == server->max_connections){
        return ERROR_CONN_FULL;     
    }

    epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, new_conn_sock_fd, new_event);

    event_tcp_init(new_event_data, new_conn_sock_fd, new_conn);
    new_event->events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP; // 不需要out事件，该事件通常的都是可以写的
    new_event->data.ptr = &new_event_data;

    server->stats.current_connections ++;
    server->stats.total_connections ++;
    
    DEBUG_TCP_SERVER("server_handle_accept_event return sucess, accept a new fd %d\n", new_conn_sock_fd);
    return 0;
}

int server_handle_tcp_event(tcpServer* server, Connection *conn, MemoryArena *global_arena, struct epoll_event* event){
    ASSERT(conn);
    ASSERT(server);
    ASSERT(global_arena);
    ASSERT(event);
    
    int ret = 0;

    if (event_check(event, EPOLLERR)){
        ret = ERROR_SYSTEM; goto clean_and_close;
    }

    if (event_check(event, EPOLLIN)){
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
                ret = ERROR_PROTO; 
                goto clean_and_close;
            }
        }else if (parse_ret == 0){ // 正确读入
            DEBUG_TCP_SERVER("server_handle_tcp_event protocol read sucess\n");
            int process_ret = handler->on_process(conn); // 处理
            if (process_ret != 0){
                ret = ERROR_PROTO; goto clean_and_close;
            }
            DEBUG_TCP_SERVER("server_handle_tcp_event: protocol process sucess\n");
        }
    }

    if (event_check(event, EPOLLOUT)){
        if (connection_send(conn, NULL) < 0){
            DEBUG_TCP_SERVER("server_handle_tcp_event: system error\n");
            ret = ERROR_SYSTEM;
            goto clean_and_close;
        }
    }

    if (event_check(event, EPOLLRDHUP | EPOLLHUP)) {
        epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, conn->fd, event);
        tcp_server_close_connection(server, conn); // 正常关闭
        DEBUG_TCP_SERVER("server_handle_tcp_event is close and clean\n");   
        return ERROR_TCP_CLOSE; 
    }

    DEBUG_TCP_SERVER("server_handle_tcp_event is over\n");
    return 0;
clean_and_close:
    DEBUG_TCP_SERVER("server_handle_tcp_event current error, clean and close\n");
    tcp_server_close_connection(server, conn);;
    return ret;
}

int server_handle_time_event(){

}


/* 服务器关闭一个connction_t连接并且reset */
void tcp_server_close_connection(tcpServer* server, Connection* conn){
    if (!conn) return;
    
    connection_clear_protocol(conn);
    
    server->stats.current_connections--;
    connection_close(conn);
}

