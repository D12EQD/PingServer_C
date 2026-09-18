/*
 * 该内存块采用bitmap + 尾部标记的方法分配大块内存
 * 采用引用计数分配小块内存
 */
#include <stdlib.h>
#include <stdint.h>

#ifndef PING_ARENA_LIMIT_BLOCK
    #define PING_ARENA_LIMIT_BLOCK 2048 // 2048 * 4096
#endif

#define PING_ARENA_BLOCK_LOG_SIZE 12
#define PING_ARENA_BLOCK_SIZE (1 << PING_ARENA_BLOCK_LOG_SIZE) /* default page size */

typedef struct Block_t{
    uint64_t empty[PING_ARENA_BLOCK_SIZE / sizeof(uint64_t)];
}Block;

typedef struct MemoryArena_s{
    Block* start;
    uint8_t* bitmap;        
    uint8_t* ref_count;     // 对部分块采用引用计数，专门针对小对象分配
    uint16_t* ref_len;       // 当前 ref block 可以分配的地址
    uint64_t now;           // 当前所在 start 的位置
    uint64_t cap;           // block 容量
} MemoryArena;

void arena_free(MemoryArena *arena);
void arena_recycle(MemoryArena *arena,void *ptr);
MemoryArena *arena_create(size_t size);
void arena_clean(MemoryArena* arena);
void* arena_alloc_block(MemoryArena* arena, size_t block_count, size_t* return_size);
void* arena_alloc_ref_block(MemoryArena* arena, size_t size);
void arena_recycle_ref(MemoryArena* arena, void *ptr);
// Public
#define arena_alloc(arena, cap, return_ptr) arena_alloc_block(arena, (cap + PING_ARENA_BLOCK_SIZE) / (PING_ARENA_BLOCK_LOG_SIZE), return_ptr)
