#pragma once
#include <bits/types/struct_itimerspec.h>
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
    void (*on_read)(void *ctx); // 可读回调
    void (*on_write)(void *ctx);// 可写回调
    void (*on_error)(void *ctx);// 错误回调
    uint8_t e_type;
    int e_fd;
    int error_reason;
} Event;

typedef struct {
    Event e;
    Connection * conn;
    MemoryArena *global_arena;
    void * server;
} EventTcpContext;

typedef struct {
    Event e;
    uint64_t out_time; // 超时时间 单位为ms
    EventTcpContext* tcp_event;
    void * server;
} EventTimerContext;

typedef struct {
    Event e;
    void *server;
} EventListenContext;

struct EventOtherArg{
    int fd;
};

// 添加一个事件
#define event_loop_add(epfd, ctx, ev, ...) event_loop_add_imple(( epfd ), ( ctx ), ev)

void event_bind(void* e, Event *base_e);
int event_loop_add_imple(int epfd, Event* ctx, struct epoll_event* ev);
int event_loop_run(int epfd, int timeout_ms);
int event_loop_del(int epoll_fd, int _fd);

int event_listen_add(int epfd, EventListenContext* ctx, struct epoll_event * ev);
int event_timer_add(int epfd, EventTimerContext * ctx, struct epoll_event * ev);

