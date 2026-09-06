#include <stdint.h>
#include <stdlib.h>
#include "ds/ping_arena.h"
#include "ds/bitmap.h"
#include "other/debug.h"
#include "other/def.h"

MemoryArena* arena_create(size_t size){
    if (size == 0) size =  PING_ARENA_BLOCK_SIZE;

    MemoryArena* arena = (MemoryArena*) malloc(sizeof(MemoryArena));
    
    arena->start = (Block *)malloc(size * sizeof(Block));
    arena->bitmap = (uint8_t *)malloc(size);
    
    memset(arena->bitmap, 0, (size + 7) / 8);

    arena->now = 0;
    arena->cap = size;

    return arena;
}

void* arena_alloc_block(MemoryArena* arena, size_t block_count, size_t* return_size){
    ASSERT(arena); 
    ASSERT(block_count <= arena->cap);

    size_t limit = arena->cap;
    size_t found_idx = (size_t) -1;
    size_t consecutive = 0;
    size_t candidate_start = 0;

    for (size_t i = 0; i < limit; i ++) {
        size_t idx = (arena->now + i) % limit;
        
        if (likely(BIT_GET(arena->bitmap, idx) == 0)) {
            if (consecutive == 0) {
                candidate_start = idx;
            }

            consecutive ++;
            
            if (consecutive == block_count) {
                found_idx = candidate_start;
                break;
            }
        }else{
            consecutive = 0; 
        }

        if (unlikely(idx == limit - 1)){
            consecutive = 0;
        }
    }

    if (unlikely(found_idx == (size_t) -1)) {
        return NULL; 
    }

    bitmap_set_range(arena->bitmap, found_idx, found_idx + block_count - 1);
    
    arena->now = (found_idx + block_count) % limit;
    arena->start[found_idx].empty[0] = block_count;

    void* user_ptr = &(arena->start[found_idx].empty[1]);


    if (unlikely(return_size)){
        *return_size = block_count * PING_ARENA_BLOCK_SIZE - sizeof(uint64_t);
    }

    return user_ptr;
}

void* arena_alloc(MemoryArena* arena, size_t size){
    size_t new_size_count = (size + sizeof(uint64_t) + PING_ARENA_BLOCK_SIZE - 1) >> PING_ARENA_BLOCK_LOG_SIZE;
    return arena_alloc_block(arena, new_size_count, NULL);
}

/**
 * @brief 回收 block 设置为空闲
 * @param arena 
 * @param ptr 用户层传入的指针 (即 arena_alloc 返回的指针)
 */
void arena_recycle(MemoryArena* arena, void* ptr){
    Block* block = (Block*)((uint8_t*)ptr - sizeof(uint64_t));
    
    uint64_t block_count = *((uint64_t*)block);
    
    size_t start_idx = ((uint8_t*)block - (uint8_t*)arena->start) / PING_ARENA_BLOCK_SIZE;

    bitmap_unset_range(arena->bitmap, start_idx, start_idx + block_count - 1);

    // 可选：更新 arena->now
    if (start_idx < arena->now) {
        arena->now = start_idx;
    }
}

void arena_free(MemoryArena *arena){
    free(arena->bitmap);
    free(arena->start);
    free(arena);
}

void arena_clean(MemoryArena* arena){
    memset(arena->bitmap, 0, (arena->cap + 7) / 8);
    arena->now = 0;
}