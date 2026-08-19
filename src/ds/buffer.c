#include "ds/buffer.h"

void buffer_init(buffer_t *buf, void *mem_ptr, uint32_t cap, void *pool_ctx) {
    if (!buf) return;
    buf->data = (uint8_t *)mem_ptr;
    buf->cap = cap;
    buf->r_idx = 0;
    buf->w_idx = 0;
    buf->pool_ctx = pool_ctx;
}

// 消费 len 字节数据（推进读游标）
void buffer_retrieve(buffer_t *buf, size_t len) {
    if (!buf) return;

    // 可读数据量计算
    uint32_t readable = buf->w_idx - buf->r_idx;
    if (len >= readable) {
        buf->r_idx = 0;
        buf->w_idx = 0; // ? 为什么这要设置为0，全部读完了对吗？
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

// 碎片整理：将 [r_idx, w_idx) 的未读数据移动到内存块头部 [0, len)
void buffer_adjust(buffer_t *buf) {
    if (!buf || buf->r_idx == 0) return;

    uint32_t readable = buf->w_idx - buf->r_idx;
    if (readable > 0) {
        // 使用 memmove 规避重叠内存拷贝的安全隐患
        memmove(buf->data, buf->data + buf->r_idx, readable);
    }
    
    buf->r_idx = 0;
    buf->w_idx = readable;
}

// 释放/重置 Buffer 结构体
void buffer_free(buffer_t *buf) {
    if (!buf) return;

    // 注意：pool_ctx 与 data 的回收逻辑由你的内存池接口负责
    buf->data = NULL;
    buf->cap = 0;
    buf->r_idx = 0;
    buf->w_idx = 0;
}