#pragma once
#include <stdint.h>
#include "ds/arena.h"

typedef struct buffer {
    uint8_t *data;
    uint32_t cap;      // 总容量
    uint32_t len;      // 当前有效数据长度（从 0 开始）
    uint32_t r_pos;    // 读位置（已消费的字节数）
} buffer_t;

/* 创建和销毁 */
buffer_t* buffer_create(size_t cap);
buffer_t* buffer_create_from_arena(size_t cap, Arena *a);
void buffer_free(buffer_t *buf);

/* 读操作 */
size_t buffer_readable(const buffer_t *buf);           // 可读字节数
uint8_t* buffer_read_ptr(const buffer_t *buf);        // 读指针（不移动位置）
void buffer_read(buffer_t *buf, size_t n);            // 消费 n 字节

/* 写操作 */
size_t buffer_writable(const buffer_t *buf);          // 可写空间
uint8_t* buffer_write_ptr(buffer_t *buf);             // 写指针
void buffer_write(buffer_t *buf, size_t n);           // 确认已写入 n 字节
void buffer_append(buffer_t *buf, const void *data, size_t len);  // 追加数据（一步到位）

/* 内存管理 */
void buffer_reset(buffer_t *buf);                     // 清空所有数据和读位置
void buffer_compact(buffer_t *buf);                   // 紧凑化：移除已消费数据
int buffer_resize(buffer_t **buf, size_t new_cap, Arena *a);  // 调整容量

/* 调试 */
void buffer_print(const buffer_t *buf);
