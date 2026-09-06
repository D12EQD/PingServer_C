#include "ds/buffer.h"
#include "ds/ping_arena.h"
#include "other/debug.h"
#include <stdlib.h>

#define DEBUG_BUFFER(...) DEBUG(DEBUG_FLAG_BUFFER, ##__VA_ARGS__)

// create a buffer use malloc 
Buffer* buffer_create(size_t cap) {
    Buffer *buf = (Buffer *)malloc(sizeof(Buffer) + cap);
    if (!buf) return NULL;
    
    
    buf->cap = cap;
    buf->len = 0;
    buf->idx = 0;
    return buf;
}

// create a buffer use MemoryArena 
Buffer* buffer_create_from_arena(size_t cap, MemoryArena *a){
    Buffer *buf = (Buffer *)arena_alloc(a, sizeof(Buffer));
    
    buf->cap = cap;
    buf->len = 0;
    buf->idx = 0;
    return buf;
}

void buffer_free(Buffer *buf) {
    free(buf);
}

void buffer_clean(Buffer* buf){
    buf->idx = 0;
    buf->len = 0;
}

void buffer_free_from_arena(Buffer *buf, MemoryArena *a){
    arena_recycle(a, buf);
}
