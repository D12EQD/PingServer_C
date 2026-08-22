#include "ds/buffer.h"
#include "ds/arena.h"
#include "other/debug.h"

void buffer_print(buffer_t *buf){
    if (!buf){
        DEBUG(DEBUG_FLAG_BUFFER, "buf is null\n");
        return;
    }

    DEBUG(DEBUG_FLAG_BUFFER, 
        "\n"
        "    buf data : %p\n"
        "    buf write idx : %lu\n"
        "    buf read idx : %lu\n", buf->data, buf->w_idx, buf->r_idx
    );
}

void buffer_init(buffer_t *buf, void *mem_ptr, uint32_t cap) {
    buf->data = (uint8_t *)mem_ptr;
    buf->cap = cap;
    buf->r_idx = 0;
    buf->w_idx = 0;
}

/* 剩余的read容量 */
size_t buffer_read_cap(buffer_t *buf){
    return buf->w_idx - buf->r_idx;
}

/* 剩余的write容量 */
size_t buffer_write_cap(buffer_t *buf){
    return buf->cap - buf->w_idx;
}

/* 从arena中创建一个新内存 */
buffer_t* buffer_create_from_arena(size_t cap, Arena* a){
    buffer_t *buf = (buffer_t *)arena_alloc(a, sizeof(buffer_t));
    
    if (!buf) return NULL;
    
    uint8_t *data = (uint8_t *)arena_alloc(a, cap);

    if (!data) {
        free(buf);  
        return NULL;
    }

    buffer_init(buf, data, cap);
    return buf;
}

/* 使用malloc创建一个新的缓冲区 */
buffer_t* buffer_create(size_t cap) {
    buffer_t *buf = (buffer_t *)malloc(sizeof(buffer_t));
    if (!buf) return NULL;
    uint8_t *data = (uint8_t *)malloc(cap);
    if (!data) {
        free(buf); 
        return NULL;
    }

    buffer_init(buf, data, cap);
    return buf;
}

/* 消费 len 字节数据*/
void buffer_retrieve(buffer_t *buf, size_t len) {
    if (!buf) return;

    // 可读数据量计算
    uint32_t readable = buf->w_idx - buf->r_idx;
    if (len >= readable) {
        buf->r_idx = 0;
        buf->w_idx = 0; 
    } else {
        buf->r_idx += (uint32_t)len; 
    }
}

// Socket/应用层写入 len 字节数据（推进写游标）
void buffer_has_written(buffer_t *buf, size_t len) {
    if (!buf) return;

    // 防止写游标越界
    uint32_t writable = buf->cap - buf->w_idx;
    if (len > writable) {
        len = writable;
    }
    buf->w_idx += (uint32_t)len;
}

// 零拷贝预读：返回未消费数据的首地址，不移动 r_idx
uint8_t* buffer_peek(buffer_t *buf) {
    if (!buf || buf->r_idx >= buf->w_idx) {
        return NULL; // 没有可读数据
    }
    return buf->data + buf->r_idx;
}


// 释放/重置 Buffer 结构体
void buffer_reset(buffer_t *buf) {
    if (!buf) return;
    buf->r_idx = 0;
    buf->w_idx = 0;
}


void buffer_free(buffer_t *buf){
    free(buf->data);
    free(buf);
}