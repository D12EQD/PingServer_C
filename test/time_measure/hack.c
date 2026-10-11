#include "ds/ping_arena.h"
#include "other/global_time.h"

uint64_t cost_time = 0; // ms

void hack_arena_recycle(MemoryArena *arena,void *ptr){
    uint64_t now =  global_get_time();
    arena_recycle(arena, ptr);
    cost_time += now - global_get_time();
}

void hack_arena_clean(MemoryArena* arena){
    uint64_t now =  global_get_time();
    arena_clean(arena);
    cost_time += now - global_get_time();
}

void* hack_arena_alloc_block(MemoryArena* arena, size_t block_count, size_t* return_size){
    uint64_t now =  global_get_time();
    void *res = arena_alloc_block(arena, block_count, return_size);
    cost_time += now - global_get_time();
    return res;
}

void* hack_arena_alloc_ref_block(MemoryArena* arena, size_t size){
    uint64_t now =  global_get_time();
    void *res = arena_alloc_ref_block(arena, size);
    cost_time += now - global_get_time();
    return res;
}

void hack_arena_recycle_ref(MemoryArena* arena, void *ptr){
    uint64_t now =  global_get_time();
    arena_recycle_ref(arena, ptr);
    cost_time += now - global_get_time();
}


