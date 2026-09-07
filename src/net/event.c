#define _GNU_SOURCE  
#include <sys/epoll.h>
#include "net/event.h"
#include <sys/timerfd.h>

int event_timer_add(int epfd, EventTimerContext * ctx, struct epoll_event * ev){
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);   
    timerfd_settime(timer_fd, 0, &ctx->timer_spec, NULL);
    ev->events = EPOLLIN;
    ev->data.fd = timer_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, timer_fd, ev);
    return 0;
}

int event_tcp_add(int epfd, EventTcpContext* ctx, struct epoll_event * ev){
    
}
