#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct buffer {
    uint8_t *data; // 指向内存池分配的内存块首地址
    void *pool_ctx; // 绑定的内存池上下文/归还句柄（可选）
    uint32_t cap; // 内存块容量
    uint32_t r_idx; // 读游标
    uint32_t w_idx; // 写游标 
}buffer_t;
