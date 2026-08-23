#include "ds/buffer.h"
#include "other/debug.h"
#include <stdlib.h>

#define DEBUG_BUFFER(...) DEBUG(DEBUG_FLAG_BUFFER, ##__VA_ARGS__)

buffer_t* buffer_create(size_t cap) {
    buffer_t *buf = (buffer_t *)malloc(sizeof(buffer_t));
    if (!buf) return NULL;
    
    uint8_t *data = (uint8_t *)malloc(cap);
    if (!data) {
        free(buf);
        return NULL;
    }
    
    buf->data = data;
    buf->cap = cap;
    buf->len = 0;
    buf->r_pos = 0;
    return buf;
}

buffer_t* buffer_create_from_arena(size_t cap, Arena *a) {
    buffer_t *buf = (buffer_t *)arena_alloc(a, sizeof(buffer_t));
    if (!buf) return NULL;
    
    uint8_t *data = (uint8_t *)arena_alloc(a, cap);
    if (!data) return NULL;
    
    buf->data = data;
    buf->cap = cap;
    buf->len = 0;
    buf->r_pos = 0;
    return buf;
}

void buffer_free(buffer_t *buf) {
    if (!buf) return;
    free(buf->data);
    free(buf);
}

/* ===== 读操作 ===== */
size_t buffer_readable(const buffer_t *buf) {
    if (!buf) return 0;
    return buf->len - buf->r_pos;
}

uint8_t* buffer_read_ptr(const buffer_t *buf) {
    if (!buf) return NULL;
    return buf->data + buf->r_pos;
}


void buffer_read(buffer_t *buf, size_t n) {
    if (!buf) return;
    size_t readable = buffer_readable(buf);
    if (n > readable) n = readable;
    buf->r_pos += n;
}

/* ===== 写操作 ===== */
size_t buffer_writable(const buffer_t *buf) {
    if (!buf) return 0;
    return buf->cap - buf->len;
}

uint8_t* buffer_write_ptr(buffer_t *buf) {
    if (!buf) return NULL;
    return buf->data + buf->len;
}

void buffer_write(buffer_t *buf, size_t n) {
    if (!buf) return;
    size_t writable = buffer_writable(buf);
    if (n > writable) n = writable;
    buf->len += n;
}

void buffer_append(buffer_t *buf, const void *data, size_t len) {
    if (!buf || !data) return;
    
    size_t writable = buffer_writable(buf);
    if (len > writable) {
        len = writable;  // 截断写入
    }
    
    memcpy(buffer_write_ptr(buf), data, len);
    buffer_write(buf, len);
}

/* ===== 内存管理 ===== */
void buffer_reset(buffer_t *buf) {
    if (!buf) return;
    buf->len = 0;
    buf->r_pos = 0;
}

void buffer_compact(buffer_t *buf) {
    if (!buf || buf->r_pos == 0) return;
    
    size_t readable = buffer_readable(buf);
    if (readable > 0) {
        memmove(buf->data, buffer_read_ptr(buf), readable);
    }
    buf->len = readable;
    buf->r_pos = 0;
}

int buffer_resize(buffer_t **buf, size_t new_cap, Arena *a) {
    if (!buf || !*buf) return -1;
    
    buffer_t *old_buf = *buf;
    size_t readable = buffer_readable(old_buf);
    
    // 创建新缓冲区
    buffer_t *new_buf = buffer_create_from_arena(new_cap, a);
    if (!new_buf) return -1;
    
    // 拷贝可读数据
    if (readable > 0) {
        memcpy(new_buf->data, buffer_read_ptr(old_buf), readable);
        new_buf->len = readable;
    }
    
    free(old_buf->data);
    free(old_buf);
    *buf = new_buf;
    return 0;
}

void buffer_print(const buffer_t *buf) {
    if (!buf) {
        DEBUG_BUFFER("buffer: NULL\n");
        return;
    }
    DEBUG_BUFFER("buffer {\n"
           "  data: %p\n"
           "  cap: %u\n"
           "  len: %u (written)\n"
           "  r_pos: %u (read position)\n"
           "  readable: %zu bytes\n"
           "  writable: %zu bytes\n"
           "}\n",
        buf->data, buf->cap, buf->len, buf->r_pos,
        buffer_readable(buf), buffer_writable(buf)
    );
}
