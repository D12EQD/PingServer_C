// include/ds/event.h - 已有，完善它

#include <stdlib.h>
#include <stdint.h>

typedef struct {
    int fd;                    // 文件描述符
    uint32_t events;          // 监听的事件（EPOLLIN | EPOLLOUT）
    void *data;               // 关联的业务数据
} Event;

