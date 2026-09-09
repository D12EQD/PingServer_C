#define _GNU_SOURCE  
#include <sys/epoll.h>
#include "net/event.h"
#include <sys/timerfd.h>

#include "net/tcp_server.h"

struct epoll_event events[1024];

static inline int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 增加一个计时器事件
// 会自动设置事件为非阻塞的
// 0 is OK, -1 is error
int event_timer_add(int epfd, EventTimerContext * ctx, struct epoll_event * ev){
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);   
    timerfd_settime(timer_fd, 0, &ctx->timer_spec, NULL);
    ev->events = EPOLLIN | EPOLLET ; // 非阻塞设置
    ev->data.ptr = ctx;

    if (set_nonblocking(timer_fd) < 0) return -1;
    epoll_ctl(epfd, EPOLL_CTL_ADD, timer_fd, ev);
    return 0;
}

// 增加一个tcp事件，一般用于服务器listen事件发生后需要注册一个新事件来维护和用户的联系
// 会自动设置事件为非阻塞的
// 0 is OK, -1 is error
int event_tcp_add(int epfd, EventTcpContext* ctx, struct epoll_event * ev){
    int fd = ctx->conn->fd;
    
    if (set_nonblocking(fd) < 0) return -1;

    ev->events = EPOLLIN | EPOLLET | EPOLLRDHUP; // 非阻塞设置
    ev->data.ptr = ctx; // 这里应指向包含公共基类的ctx
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, ev);
}

// 增加一个listen事件，服务器一开始创建时需要调用这个
// 会自动设置事件为非阻塞的
// 0 is OK, -1 is error
int event_accept_add(int epfd, EventListenContext* ctx, struct epoll_event * ev){
    ev->events = EPOLLIN | EPOLLET | EPOLLRDHUP; // 非阻塞设置
    ev->data.ptr = ctx;

    int fd = ( (tcpServer *)ctx->server ) -> listen_fd;
    if (set_nonblocking(fd) < 0) return -1;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, ev);

    return 0;
}

// 将事件e 中的回调函数设置为和base_e相同的
void event_bind(void* e, Event *base_e){
    memcpy(e, base_e, sizeof(Event));
}

int event_loop_add(int epfd, Event* ctx, struct epoll_event* ev){
    switch (ctx->e_type){
        case EVENT_TYPE_TIMER:
            return event_timer_add(epfd, (void *)ctx,ev);
        case EVENT_TYPE_TCP:
            return event_tcp_add(epfd, (void*)ctx, ev);
        case EVENT_TPYE_LISTEN:
            return event_accept_add(epfd, (void *)ctx, ev);
        default:
            return -1;
    }
    return -1;
}

int event_loop_run(int epoll_fd, int timeout_ms) {
    int nfds = epoll_wait(epoll_fd, events, 1024, timeout_ms);
    if (nfds == -1) return -1;

    for (int i = 0; i < nfds; i++) {
        Event *ev = (Event *)events[i].data.ptr;
        if (!ev) continue;

        if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            if (ev->on_error) ev->on_error(ev);
        } else {
            if (events[i].events & EPOLLIN) {
                if (ev->on_read) ev->on_read(ev);
            }
            if (events[i].events & EPOLLOUT) {
                if (ev->on_write) ev->on_write(ev);
            }
        }
    }
    return 0;
}
