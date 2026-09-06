#pragma once
#include <stdint.h>
#include <stdlib.h>
#include "ds/ping_arena.h"

typedef struct buffer {
    uint32_t cap;      // 总容量
    uint32_t len;      // 当前有效数据长度（从 0 开始）
    uint32_t idx;    // 读位置（已消费的字节数）
    uint8_t data[];
} Buffer;

/* 创建和销毁 */
Buffer* buffer_create(size_t cap);
Buffer* buffer_create_from_arena(size_t cap, MemoryArena *a);
void buffer_free(Buffer *buffer);
void buffer_clean(Buffer* buf);
void buffer_free_from_arena(Buffer *buf, MemoryArena *a);
