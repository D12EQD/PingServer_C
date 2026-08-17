/*
* 当前问题：不知道为什么
*/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include "core/ping_alloc.h"

#define NUM_THREADS 4
#define ITERATIONS_PER_THREAD 1000
#define MAX_ALLOC_SIZE 4096

void __attribute__((constructor)) all_init(void){
    debug_statistics_list_init();
    ping_alloc_debug_init();
}

void __attribute__((destructor)) all_free(void){
    debug_statistics_list_free();
}

// 辅助结构体：用于记录分配指针及其大小
typedef struct {
    void *ptr;
    size_t size;
    uint8_t pattern;
} alloc_record_t;

// 1. 跨边界与
static void test_alignment_and_boundaries(ping_alloc_t *alloc) {
    printf("[进阶测试 1] 奇数/不规则尺寸测试...\n");
    
    // 测试 1 字节到奇数尺寸的分配
    size_t test_sizes[] = {1, 3, 7, 15, 31, 63, 1023, 1025, 4095, 4097};
    size_t num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);
    void *ptrs[10];

    for (size_t i = 0; i < num_sizes; i++) {
        ptrs[i] = ping_malloc(alloc, test_sizes[i]);
        assert(ptrs[i] != NULL);
        
        // 填充边界：测试写满申请的最后一个字节
        memset(ptrs[i], (int)(i + 0x55), test_sizes[i]);
    }

    // 校验所有数据完整性（确保相邻分配未发生内存踩踏）
    for (size_t i = 0; i < num_sizes; i++) {
        unsigned char *p = (unsigned char *)ptrs[i];
        for (size_t j = 0; j < test_sizes[i]; j++) {
            assert(p[j] == (unsigned char)(i + 0x55));
        }
    }
}

// 2. 疯狂抖动与内存碎片化测试 (Stress & Fragmentation)
static void test_stress_and_fragmentation(ping_alloc_t *alloc) {
    printf("[进阶测试 2] 高频随机分配/释放与碎片化压力测试...\n");
    
    #define RECORD_COUNT 500
    alloc_record_t records[RECORD_COUNT] = {0};

    // 随机种子固定，便于复现 Bug
    srand(42);

    for (int iter = 0; iter < 10000; iter++) {
        int idx = rand() % RECORD_COUNT;

        if (records[idx].ptr != NULL) {
            // 校验已有数据是否被非法覆盖
            unsigned char *p = (unsigned char *)records[idx].ptr;
            for (size_t j = 0; j < records[idx].size; j++) {
                assert(p[j] == records[idx].pattern);
            }
            
            // 模拟 30% 概率做 realloc，70% 概率直接不释放改写/或做分配
            if (rand() % 100 < 30) {
                size_t new_size = (rand() % MAX_ALLOC_SIZE) + 1;
                uint8_t new_pattern = (uint8_t)(rand() % 256);
                
                void *new_ptr = ping_realloc(alloc, records[idx].ptr, records[idx].size, new_size);
                assert(new_ptr != NULL);
                
                // 校验原有前 min(old_size, new_size) 字节是否保留
                size_t check_len = records[idx].size < new_size ? records[idx].size : new_size;
                p = (unsigned char *)new_ptr;
                for (size_t j = 0; j < check_len; j++) {
                    assert(p[j] == records[idx].pattern);
                }
                
                // 刷新记录
                records[idx].ptr = new_ptr;
                records[idx].size = new_size;
                records[idx].pattern = new_pattern;
                memset(records[idx].ptr, new_pattern, new_size);
            }
        } else {
            // 申请新内存
            size_t size = (rand() % MAX_ALLOC_SIZE) + 1;
            uint8_t pattern = (uint8_t)(rand() % 256);
            
            void *ptr = ping_malloc(alloc, size);
            assert(ptr != NULL);
            
            records[idx].ptr = ptr;
            records[idx].size = size;
            records[idx].pattern = pattern;
            memset(ptr, pattern, size);
        }
    }
}

// 3. 引用计数与交错生命周期测试
static void test_refcount_interleaving(ping_alloc_t *alloc) {
    printf("[进阶测试 3] 大块（region_big_t）复杂的引用计数与生命周期交错...\n");

    // 模拟多处共享同一 region_big_t 块
    void *b1 = ping_malloc(alloc, 8192);
    void *b2 = ping_malloc(alloc, 16384);

    assert(b1 != NULL && b2 != NULL);

    // 建立交叉引用
    ping_alloc_big_ref(b1); // b1 ref = 2
    ping_alloc_big_ref(b2); // b2 ref = 2
    ping_alloc_big_ref(b1); // b1 ref = 3

    // 释放顺序错位
    ping_alloc_big_unref(alloc->big_rlist, b2); // b2 ref = 1
    ping_alloc_big_unref(alloc->big_rlist, b1); // b1 ref = 2
    
    // 此时 reset 小块链表，大块不应受影响
    ping_alloc_rlist_reset(alloc);
    
    ping_alloc_big_unref(alloc->big_rlist, b2); 
}

int main() {
    DEBUG_FLAG_SET(DEBUG_FLAG_ALL);

    ping_alloc_t *alloc = ping_alloc_create();
    assert(alloc != NULL);

    // 依次执行高强度测试集
    // test_alignment_and_boundaries(alloc);
    test_stress_and_fragmentation(alloc);
    // test_refcount_interleaving(alloc);

    ping_alloc_free(alloc);

    printf("\n🔥 所有高复杂度专项压力测试顺利通过！\n");
    return 0;
}

