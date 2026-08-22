#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "ds/arena.h"

typedef struct buffer {
    uint8_t *data; // 指向内存池分配的内存块首地址
    uint32_t cap; // 内存块容量
    uint32_t r_idx; // 读游标
    uint32_t w_idx; // 写游标 
}buffer_t;


buffer_t* buffer_create_from_arena(size_t cap, Arena* a);
buffer_t* buffer_create(size_t cap);
void buffer_init(buffer_t *buf, void *mem_ptr, uint32_t cap);
void buffer_retrieve(buffer_t *buf, size_t len);
void buffer_has_written(buffer_t *buf, size_t len);
uint8_t* buffer_peek(buffer_t *buf);
void buffer_adjust(buffer_t *buf) ;
void buffer_reset(buffer_t *buf);
size_t buffer_read_cap(buffer_t *buf);
size_t buffer_write_cap(buffer_t *buf);
void buffer_free(buffer_t *buf);