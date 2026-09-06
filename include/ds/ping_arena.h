#include <stdlib.h>
#include <stdint.h>

#define PING_ARENA_LIMIT_BLOCK 2048 // 2048 * 4096
#define PING_ARENA_BLOCK_LOG_SIZE 12
#define PING_ARENA_BLOCK_SIZE (1 << PING_ARENA_BLOCK_LOG_SIZE) /* default page size */

typedef struct Block_t{
    uint64_t empty[PING_ARENA_BLOCK_SIZE / sizeof(uint64_t)];
}Block;

/**
 * @brief 
 * MemoryArena对List中的字段解释不一样
 * MemoryArena->begin 为 大块内存的起始位置, MemoryArena->end 为 当前可以分配的内存区域位置
 */
typedef struct MemoryArena_s{
    Block* start;
    uint8_t* bitmap;
    uint64_t now;
    uint64_t cap;
} MemoryArena;

void arena_free(MemoryArena *arena);
void arena_recycle(MemoryArena *arena,void *ptr);
void *arena_alloc(MemoryArena *arena,size_t block_count);
MemoryArena *arena_create(size_t size);
void arena_clean(MemoryArena* arena);
void* arena_alloc_block(MemoryArena* arena, size_t block_count, size_t* return_size);