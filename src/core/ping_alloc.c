#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stddef.h>

#include "other/debug.h"
#include "ds/linklist.h"
#include "core/ping_alloc.h"

#ifdef PINGNET_DEBUG_ENABLE
    #define DEBUG_ALLOC(...) DEBUG(DEBUG_FLAG_ALLOC, ##__VA_ARGS__); fflush(stdout);
#else
    #define DEBUG_ALLOC(...) (void(0))
#endif

#define region_ptr(t) ((region_t *)(t))
#define region_big_ptr(t) ((region_big_t *)(t))

// 固定分配大小为默认为 8192 byte
#ifndef PING_DEFAULT_ALLOC_SIZE
    #define PING_DEFAULT_ALLOC_SIZE 8192 
#endif

static inline region_t* region_create(); 
static inline region_big_t* region_big_create(size_t size, uint16_t ref);
static inline region_big_t *region_big_from_data(void *ptr);

/*
* 注册debug_statistic信息
* 在debug模式下统计相关数据
* 注意需要先使用region_debug_init函数进行
* How many times new region was create
* How many times existing region was skipped
* How many times allocation exceeded ARENA_REGION_DEFAULT_CAPACITY
*/
typedef enum {
    STAT_ALLOC_SKIP,
    STAT_ALLOC_EXCEEDED,
    STAT_ALLOC_NEW_REGION,
    STAT_COUNT  // 总个数，放在最后
} debug_stat_id_t;

static const char *stat_names[] = {
    [STAT_ALLOC_SKIP]        = "alloc_skip",
    [STAT_ALLOC_EXCEEDED]    = "alloc_exceeded_default",
    [STAT_ALLOC_NEW_REGION]  = "alloc_new_region",
};

static debug_statistics_t *g_debug_stats[STAT_COUNT];

void ping_alloc_debug_init(){
    for (uint8_t i = 0; i < STAT_COUNT; i ++){
        g_debug_stats[i] = debug_statistics_register(stat_names[i]);
    }
}

region_t* region_create(){
    size_t all_size = PING_DEFAULT_ALLOC_SIZE + sizeof(region_t);
    region_t *r = (region_t *)malloc(all_size);
    if (!r) return NULL;
    
    memset(r, 0, all_size);
    r->capacity = PING_DEFAULT_ALLOC_SIZE;

    DEBUG_ALLOC("create a region %p , data range [%p, %p], len is %d\n", 
        r, r->data , r->data + PING_DEFAULT_ALLOC_SIZE, r->capacity
    );
    return r;
}

region_big_t* region_big_create(size_t size, uint16_t ref){
    size_t all_size = sizeof(region_big_t) + size;
    region_big_t *r = (region_big_t *)malloc(all_size);
    
    memset(r, 0, all_size);
    r->capacity = size;
    r->ref = ref;

    DEBUG_ALLOC("create a big region %p , data range [%p, %p], len is %d\n", 
        r, r->data , r->data + PING_DEFAULT_ALLOC_SIZE, r->capacity
    );

    return r;
}

static inline region_big_t *region_big_from_data(void *ptr){
    if (!ptr) return NULL;
    return (region_big_t *)((uint8_t *)ptr - offsetof(region_big_t, data));
}

/*
* 创建分配器
* 本质是两个链表
* 使用malloc分配内存
*/
ping_alloc_t * ping_alloc_create(){
    ping_alloc_t * p = (ping_alloc_t *)malloc(sizeof(ping_alloc_t));
    p->big_rlist = (link_list_t *)malloc(sizeof(link_list_t));
    p->rlist = (link_list_t *)malloc(sizeof(link_list_t));
    link_list_init(p->big_rlist);    
    link_list_init(p->rlist);    
    return p;
}

/*
* 清空区域分配链表
* 并且free P本身
*/
void ping_alloc_free(ping_alloc_t *p){
    link_list_free(p->big_rlist);
    link_list_free(p->rlist);
    free(p);
}

static inline void *region_alloc(link_list_t *list, size_t size){
    // 为空时插入一个新region
    if (list_is_empty(list)) {
        DEBUG_ALLOC("list is empty, insert a new region\n");

        list_insert_back(list, region_create()); 
        
        debug_statistics_trigger(g_debug_stats[STAT_ALLOC_NEW_REGION]);
        list->now = list->begin;
    }

    while (list->now->next != NULL) {
        if (region_ptr(list->now)->count + size <= region_ptr(list->now)->capacity){
            DEBUG_ALLOC("find a region");    
            break;
        }
        debug_statistics_trigger(g_debug_stats[STAT_ALLOC_SKIP]);
        list->now = list->now->next;
    }

    // 最后一个不满足情况，新创建一个region并且插入链表尾部，保证now是可以分配的指针
    // 会造成内存浪费，这是设计抉择
    if ((region_ptr(list->now))->count + size > (region_ptr(list->now))->capacity) {
        DEBUG_ALLOC("the last region is dissatisfy , create a new region\n");
        ASSERT(list->now->next == NULL);

        list_insert_back(list, region_create()); 

        list->now = list->now->next;
    }

    void *result = &(region_ptr(list->now)->data[region_ptr(list->now)->count]);
    region_ptr(list->now)->count += size;
    return result;
}

static inline void *region_big_alloc(link_list_t* list, size_t size, uint16_t used){
    if (list_is_empty(list)) {
        DEBUG_ALLOC("big list is empty, insert a new big region\n");
        list_insert_back(list, region_big_create(size, used)); 
        list->now = list->begin;
    }

    int max_loop = 10; // 找10个就不找了
    while (region_big_ptr(list->now)->ref && region_big_ptr(list->now)->capacity < size && list->now->next != NULL) {
        list->now = list->now->next;
        if (max_loop -- == 0) break; 
    }

    // 如果没找到直接新创建一个
    if (max_loop == 0){
        DEBUG_ALLOC("the last region is dissatisfy , create a new big region\n");
        list_insert_back(list, region_big_create(size, used)); 
        list->now = list->now->next;
    }

    void *result = &(region_big_ptr(list->now)->data[region_big_ptr(list->now)->count]);
    region_big_ptr(list->now)->count += size;
    region_big_ptr(list->now)->ref = 1;

    DEBUG_ALLOC("region_big_alloc return result\n");
    return result;
}

/*
* 分配内存，需要指定需要插入的区域内存管理器ping_alloc_t，单位为B
*/
void* ping_malloc(ping_alloc_t *p, size_t size){
    if (size <= PING_DEFAULT_ALLOC_SIZE){
        DEBUG_ALLOC("ping_malloc allocate small memeory %lu\n", size);
        return region_alloc(p->rlist, size);
    }else{
        DEBUG_ALLOC("ping_malloc allocate big memeory %lu\n", size);
        debug_statistics_trigger(g_debug_stats[STAT_ALLOC_EXCEEDED]);
        return region_big_alloc(p->big_rlist, size, 1);
    }
}

void *ping_realloc(ping_alloc_t* p, void *old_ptr, size_t old_size, size_t new_size){
    if (new_size <= old_size){
        return old_ptr;
    }

    void *new_ptr;

    if (new_size <= PING_DEFAULT_ALLOC_SIZE){
        new_ptr = region_alloc(p->rlist, new_size);
    }else{
        // TODO: 难点所在，不知道old_ptr是否是big_rlist中的内存
        // O(n) 算法
        // 尝试找到 old_size 并且copy其的引用计数

        uint16_t ref = 0;
        for (link_list_base_node_t *i = (void *)p->big_rlist; i; i = i->next){
            region_big_t *r = (void *)i;
            if ((void *)(r->data) >= (void *)old_ptr && (void *)(r->data + r->capacity) <= (void *)(old_ptr + old_size)){
                ref = r->ref;
                break;
            }
            if ((void *)i == (void *)p->big_rlist->now) break;
        }

        new_ptr = region_big_alloc(p->big_rlist, new_size, max(ref, 1));
    }

    char *new_ptr_char = (char*)new_ptr;
    char *old_ptr_char = (char*)old_ptr;

    for (size_t i = 0; i < old_size; ++i) {
        new_ptr_char[i] = old_ptr_char[i];
    }
    return new_ptr;
}

/*
* 对ping_alloc 中的 list 删除 now 后的所有空闲指针
*/
static inline void ping_alloc_rlist_trim(link_list_t *rlist){
    if (!rlist) return;
    link_list_base_node_t *i = rlist->now->next;
    link_list_base_node_t *j;

    while (i){
        j = i;
        i = i->next;
        free(j);
    }
}

void ping_alloc_big_rlist_reset(ping_alloc_t *p){
    link_list_t *rlist = p->big_rlist;
    DEBUG_ALLOC("ping_alloc_big_rlist_reset : \n");
    for (link_list_base_node_t *i = rlist->begin; i; i = i->next) {
        DEBUG_ALLOC("%p \n", i);
        ((region_big_t *)i) -> count = 0;
        ((region_big_t *)i) -> ref = 0;
    }
    DEBUG_ALLOC("print finished\n");
}

void ping_alloc_rlist_reset(ping_alloc_t *p){
    link_list_t *rlist = p->rlist;
    DEBUG_ALLOC("ping_alloc_rlist_reset : \n");
    for (link_list_base_node_t *i = rlist->begin; i; i = i->next) {
        DEBUG_ALLOC("%p \n", i);
        ((region_t *)i) -> count = 0;
    }
    DEBUG_ALLOC("print finished\n");
}

void ping_alloc_trim(ping_alloc_t* p){
    if (!p){
        DEBUG_ALLOC("ping_alloc_trim : p is null\n");
        return;
    }
    ping_alloc_rlist_trim(p->rlist);
    ping_alloc_rlist_trim(p->big_rlist);
}

void ping_alloc_reset(ping_alloc_t *p){
    ping_alloc_big_rlist_reset(p);
    ping_alloc_rlist_reset(p);
}

/*
* 用于解除大内存的引用计数
*/
void ping_alloc_big_unref(link_list_t *rlist, void *ptr){
    DEBUG_ALLOC("ping_alloc_big_unref rlist is %p, ptr is %p\n", rlist, ptr);
    region_big_t *r = region_big_from_data(ptr);
    if (!r) return;

    if (-- (r->ref) == 0){
        DEBUG_ALLOC("delete and insert %p\n", r);
        link_node_delete(r);
        r -> count = 0;
        list_insert_back(rlist, r);
    }
}

/*
* 增加大块内存引用计数
* TODO : 增加debug日志统计
*/
void ping_alloc_big_ref(void *ptr){
    region_big_t *r = region_big_from_data(ptr);
    if (!r) return;

    r->ref ++;
}