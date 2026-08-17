#pragma once

#include "other/debug.h"
#include "ds/linklist.h"
#include "other/def.h"

typedef struct region {
    struct link_list_base_node node; 
    size_t count; // 指向data的空闲的指针    
    size_t capacity; // 真实容量
    uint8_t data[];  // 实际存储空间
}region_t;

typedef struct region_big {
    struct link_list_base_node node;
    size_t count; // 指向data的空闲的指针    
    size_t capacity; // 真实容量
    uint16_t ref; // 引用计数
    uint8_t data[];  // 实际存储空间
}region_big_t;

typedef struct ping_alloc {
    link_list_t *rlist;
    link_list_t *big_rlist;
} ping_alloc_t;


ping_alloc_t * ping_alloc_create();
void* ping_malloc(ping_alloc_t *p, size_t size);

void ping_alloc_big_unref(link_list_t *rlist, void *ptr);
void ping_alloc_big_ref(void *ptr);
void ping_alloc_reset(ping_alloc_t *p);
void ping_alloc_trim(ping_alloc_t *p);
void ping_alloc_big_rlist_reset(ping_alloc_t *p);
void ping_alloc_rlist_reset(ping_alloc_t *p);

void *ping_realloc(ping_alloc_t *p,void *old_ptr,size_t old_size,size_t new_size);
void *ping_malloc(ping_alloc_t *p, size_t size);

void ping_alloc_free(ping_alloc_t *p);
void ping_alloc_debug_init();
