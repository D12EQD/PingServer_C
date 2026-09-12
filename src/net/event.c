#define _GNU_SOURCE  
#include <bits/types/struct_itimerspec.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include "net/event.h"
#include "net/tcp_server.h"
#include "other/debug.h"

#define DEBUG_EVENT(...) DEBUG(DEBUG_FLAG_EVENT, ##__VA_ARGS__)

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
    ASSERT(ctx != NULL);
    
    int timer_fd = ctx->e.e_fd;
    struct itimerspec spec = {0};
    uint64_t t = ctx->out_time;

    spec.it_interval.tv_sec = t / 1000;
    spec.it_value.tv_sec = t / 1000;   

    spec.it_interval.tv_nsec = t % 1000 * 1000000;
    spec.it_value.tv_nsec = t % 1000 * 1000000;

    
    timerfd_settime(timer_fd, 0, &spec, NULL);
    ev->events = EPOLLIN | EPOLLET ; // 非阻塞设置
    ev->data.ptr = ctx;

    if (set_nonblocking(timer_fd) < 0) return -1;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, timer_fd, ev);
}

// 增加一个tcp事件，一般用于服务器listen事件发生后需要注册一个新事件来维护和用户的联系
// 会自动设置事件为非阻塞的
// 0 is OK, -1 is error
int event_tcp_add(int epfd, EventTcpContext* ctx, struct epoll_event * ev){
    ASSERT(ctx != NULL);
    int fd = ctx->conn->fd;
    
    if (set_nonblocking(fd) < 0) return -1;

    ev->events = EPOLLIN | EPOLLET | EPOLLRDHUP; // 非阻塞设置
    ev->data.ptr = ctx; // 这里应指向包含公共基类的ctx
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, ev);
}

// 增加一个listen事件，服务器一开始创建时需要调用这个
// 会自动设置事件为非阻塞的
// 0 is OK, -1 is error
int event_listen_add(int epfd, EventListenContext* ctx, struct epoll_event * ev){
    ASSERT(ctx != NULL);
    ev->events = EPOLLIN | EPOLLET | EPOLLRDHUP; // 非阻塞设置
    ev->data.ptr = ctx;

    int fd = ( (tcpServer *)ctx->server ) -> listen_fd;
    
    if (set_nonblocking(fd) < 0) return -1;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, ev);
}

// 将事件e 中的回调函数设置为和base_e相同的
void event_bind(void* e, Event *base_e){
    memcpy(e, base_e, sizeof(Event));
}

int event_loop_add_imple(int epfd, Event* ctx, struct epoll_event* ev){
    ASSERT(ctx != NULL);
    ASSERT(ev != NULL);

    switch (ctx->e_type){
        case EVENT_TYPE_TIMER:
            DEBUG_EVENT("add timer\n");
            return event_timer_add(epfd, (void *)ctx,ev);
        case EVENT_TYPE_TCP:
            DEBUG_EVENT("add tcp\n");
            return event_tcp_add(epfd, (void*)ctx, ev);
        case EVENT_TPYE_LISTEN:
            DEBUG_EVENT("add listen\n");
            return event_listen_add(epfd, (void *)ctx, ev);
        default:
            return -1;
    }
    return -1;
}

int event_loop_run(int epoll_fd, int timeout_ms) {
    int nfds = epoll_wait(epoll_fd, events, 1024, timeout_ms);
    if (nfds == -1) return -1;

    DEBUG_EVENT("event loop get %d events, now go in loop\n", nfds);

    for (int i = 0; i < nfds; i++) {
        Event *ev = (Event *)events[i].data.ptr;
        if (!ev) continue;

        DEBUG_EVENT("event dispatch\n");

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

        DEBUG_EVENT("event %d finish\n", i);
    }
    return 0;
}

int event_loop_del(int epoll_fd, int _fd){
    DEBUG_EVENT("del a event\n");
    return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, _fd, NULL);
}
