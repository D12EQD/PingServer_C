#include <stdint.h>
#include <stdlib.h>
#include "ds/ping_arena.h"
#include "ds/bitmap.h"
#include "other/debug.h"
#include "other/def.h"

// 返回一个 MemoryArena 的指针，如果传入的size为0， 则采用默认创建的大小， 见 PING_ARENA_BLOCK_SIZE
// size 为 需要分配的 Block 数量
MemoryArena* arena_create(size_t size){
    if (size == 0) size =  PING_ARENA_BLOCK_SIZE;

    MemoryArena* arena = (MemoryArena*) malloc(sizeof(MemoryArena));
    
    arena->start = (Block *)aligned_alloc(PING_ARENA_BLOCK_SIZE, size * sizeof(Block));
    arena->bitmap = (uint8_t *)calloc((size + 7) / 8, 1);
    arena->ref_len = (uint16_t* )calloc(size * sizeof(short), 1);
    arena->ref_count = (uint8_t *)calloc(size, 1);
    arena->now = 0;
    arena->cap = size;

    ASSERT(arena->start);

    return arena;
}

void* arena_alloc_block(MemoryArena* arena, size_t block_count, size_t* return_size){
    ASSERT(arena); 
    ASSERT(block_count <= arena->cap);

    size_t limit = arena->cap;
    size_t found_idx = (size_t) -1;
    size_t consecutive = 0;
    size_t candidate_start = 0;

    // 寻找一份连续的，长度大于等于 block_count 的 blocks
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
    memset(&(arena->ref_count[found_idx]), -1, block_count * sizeof(arena->ref_count[0]));
    
    // 下一次分配的idx位置
    arena->now = (found_idx + block_count) % limit;
    arena->start[found_idx].empty[0] = block_count;

    void* user_ptr = &(arena->start[found_idx].empty[1]);

    if (unlikely(return_size)){
        *return_size = block_count * PING_ARENA_BLOCK_SIZE - sizeof(uint64_t);
    }
    
    // printf("alloc some idx [%lu , %lu]\n", found_idx, found_idx + block_count - 1);
    return user_ptr;
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
    // printf("bitmap unset a range : [%lu, %lu]\n", start_idx, start_idx + block_count - 1);

    memset(&( arena->ref_count[start_idx] ), 0, block_count * sizeof(arena->ref_count[0]));
}

void arena_free(MemoryArena *arena){
    free(arena->bitmap);
    free(arena->start);
    free(arena->ref_len);
    free(arena->ref_count);
    free(arena);
}

void arena_clean(MemoryArena* arena){
    memset(arena->bitmap, 0, (arena->cap + 7) / 8);
    memset(arena->ref_len, 0, arena->cap * sizeof(arena->ref_len[0]));
    memset(arena->ref_count, 0, arena->cap * sizeof(arena->ref_count[0]));
    arena->now = 0;
}

void* arena_alloc_ref_block(MemoryArena* arena, size_t size){
    ASSERT(arena); 

    size_t limit = arena->cap;
    size_t found_idx = (size_t) -1;

    for (size_t i = 0; i < limit; i ++) {
        size_t idx = (arena->now + i) % limit;
        if (likely(BIT_GET(arena->bitmap, idx) == 0)){
            found_idx = idx;
            break;
        }else{
            if (arena->ref_count[idx] != (uint8_t)(-1) && 
                arena->ref_len[idx] + size < PING_ARENA_BLOCK_SIZE){
                found_idx = idx;
                break;
            }
        }
   }

    if (unlikely(found_idx == (size_t) -1)) {
        return NULL; 
    }

    bitmap_set_range(arena->bitmap, found_idx, found_idx);
    void* user_ptr = (void *)(&(arena->start[found_idx]) ) + arena->ref_len[found_idx];
    arena->ref_len[found_idx] += size;
    arena->ref_count[found_idx] ++;
    arena->now = (found_idx + 1) % limit;

    // printf("alloc a idx %lu , len is %u\n", found_idx, arena->ref_len[found_idx]);

    return user_ptr;
}

void arena_recycle_ref(MemoryArena* arena, void *ptr){
    size_t idx = ( ((Block *)ptr) - (arena->start) );
    // printf("recycle ref idx %lu\n", idx);

    if (-- arena->ref_count[idx] == 0){
        arena->ref_len[idx] = 0;
        bitmap_unset_range(arena->bitmap, idx, idx);
        // printf("bitmap unset a range : [%lu, %lu]\n", idx, idx);
    }
}


