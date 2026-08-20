#define _GNU_SOURCE   
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <stdint.h>   // for uint64_t

#define MAX_EVENTS 1
#define TIMEOUT_SEC 5
#define MAX_COUNT  5

struct itimerspec timer_spec;

int get_timer_fd(uint64_t sec, uint64_t nsec){
    int timer_fd = timerfd_create(CLOCK_REALTIME, 0);
    
    timer_spec.it_interval.tv_sec = sec;
    timer_spec.it_interval.tv_nsec = nsec;
    
    timer_spec.it_value.tv_sec = sec;   
    timer_spec.it_value.tv_nsec = nsec;
    timerfd_settime(timer_fd, 0, &timer_spec, NULL);
    return timer_fd;
}

struct epoll_event events_list[MAX_EVENTS];
uint64_t buffer;

int main() {
    struct epoll_event ev;
    int count = 0;

    int epoll_fd = epoll_create1(0);
    int timer_fd = get_timer_fd(2, 0);

    // 将 timer_fd 添加到 epoll 中，监听可读事件（定时器触发时变为可读）
    ev.events = EPOLLIN;
    ev.data.fd = timer_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &ev);

    printf("等待定时器事件（每 %d 秒触发一次，触发 %d 次后退出）...\n", 2, MAX_COUNT);

    // 5. 事件循环
    while (count < MAX_COUNT) {
        // 返回一个数字代表已经触发了的数组的长度
        // 非阻塞IO，无限等待
        int nfds = epoll_wait(epoll_fd, events_list, MAX_EVENTS, -1);  

        for (int i = 0; i < nfds; i++) {
            if (events_list[i].data.fd == timer_fd) {
                ssize_t s = read(timer_fd, &buffer, sizeof(buffer));

                count ++;
                printf("定时器触发 (第 %d 次)，已过期 %llu 次\n", count, (unsigned long long)buffer);
            }
        }
    }

    close(timer_fd);
    close(epoll_fd);
    printf("程序退出\n");
    return 0;
}