#pragma once
#include <time.h>
#include <sys/timerfd.h>
#include <sys/socket.h>
#include <stdint.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>   

#include "net/connection.h"


typedef enum {
    EVENT_TYPE_TCP = 1,
    EVENT_TYPE_TIMER = 2,
    EVENT_TPYE_LISTEN = 3
} EventType;

typedef struct {
    uint8_t e_type;
    void (*on_read)(void *ctx); // 可读回调
    void (*on_write)(void *ctx);// 可写回调
    void (*on_error)(void *ctx);// 错误回调
} Event;

typedef struct {
    Event e;
    Connection * conn;
    MemoryArena *global_arena;
    void * server;
    Buffer* buffer;
} EventTcpContext;

typedef struct {
    Event e;
    uint64_t out_time; // 超时时间
    Connection * conn;
    struct itimerspec timer_spec;
} EventTimerContext;

typedef struct {
    Event e;
    void *server;
} EventListenContext;

void event_tcp_init(Event* e, int fd, Connection * conn);
void event_timer_init(Event *e, uint64_t out_time, Connection * conn);
void event_listen_init(Event *e, int fd);
void event_bind(void* e, Event *base_e);
int event_loop_add(int epfd, Event* ctx, struct epoll_event* ev);
int event_loop_run(int epoll_fd, int timeout_ms) ;
