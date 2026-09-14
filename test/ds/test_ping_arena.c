#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "ds/ping_arena.h"

// 假设你有这样的宏定义将通用名映射到具体函数，如果没有，请直接调用具体函数名
// #define arena_alloc(a, size) arena_alloc_block(a, size, NULL)

/**
 * @brief 综合压力测试：大对象与小对象（Ref Block）混合交织分配与回收
 * @param a MemoryArena 指针
 */
void test3(MemoryArena *a) {
    printf("=== Start Test 3: Mixed Large & Small Alloc/Recycle Stress Test ===\n");

    const int max_ops = 1000;
    
    // 追踪大对象指针及其占用的 block 数量
    void* large_ptrs[200] = {0};
    int large_active[200] = {0};

    // 追踪小对象（Ref）指针
    void* small_ptrs[500] = {0};
    int small_active[500] = {0};

    int large_count = 200;
    int small_count = 500;

    for (int step = 0; step < max_ops; step++) {
        int choice = rand() % 4; // 0: 分配大块, 1: 回收大块, 2: 分配小块, 3: 回收小块

        if (choice == 0) {
            // 尝试分配大对象 (1 ~ 5 个 block)
            int idx = -1;
            for (int i = 0; i < large_count; i++) {
                if (!large_active[i]) { idx = i; break; }
            }
            if (idx != -1) {
                size_t b_cnt = (rand() % 5) + 1;
                size_t ret_sz = 0;
                void *ptr = arena_alloc_block(a, b_cnt, &ret_sz);
                if (ptr) {
                    large_ptrs[idx] = ptr;
                    large_active[idx] = 1;
                    // 写入一点数据测试是否越界或崩溃
                    memset(ptr, 0xAA, ret_sz); 
                }
            }
        } 
        else if (choice == 1) {
            // 尝试随机回收一个大对象
            int idx = rand() % large_count;
            if (large_active[idx] && large_ptrs[idx]) {
                arena_recycle(a, large_ptrs[idx]);
                large_ptrs[idx] = NULL;
                large_active[idx] = 0;
            }
        } 
        else if (choice == 2) {
            // 尝试分配小对象 (32 ~ 256 字节)
            int idx = -1;
            for (int i = 0; i < small_count; i++) {
                if (!small_active[i]) { idx = i; break; }
            }
            if (idx != -1) {
                size_t s_size = (rand() % 224) + 32;
                void *ptr = arena_alloc_ref_block(a, s_size);
                if (ptr) {
                    small_ptrs[idx] = ptr;
                    small_active[idx] = 1;
                    memset(ptr, 0xBB, s_size); // 写入测试
                }
            }
        } 
        else if (choice == 3) {
            // 尝试随机回收一个小对象
            int idx = rand() % small_count;
            if (small_active[idx] && small_ptrs[idx]) {
                arena_recycle_ref(a, small_ptrs[idx]);
                small_ptrs[idx] = NULL;
                small_active[idx] = 0;
            }
        }

        // 每隔一定步数打印一次 Arena 的清理/健康状况
        if (step % 200 == 0) {
            printf("[Step %d] Arena still running smoothly...\n", step);
        }
    }

    // 清理收尾：把所有还存活的指针全部回收，测试最终清理能力
    printf("cleaning\n");
    for (int i = 0; i < large_count; i++) {
        if (large_active[i] && large_ptrs[i]) {
            arena_recycle(a, large_ptrs[i]);
        }
    }
    for (int i = 0; i < small_count; i++) {
        if (small_active[i] && small_ptrs[i]) {
            arena_recycle_ref(a, small_ptrs[i]);
        }
    }

    printf("test3 pass\n");
}

int main(){
    int round = 10;
    while (round -- ){
        int val = time(0);
        printf("send val is %d\n", val);
        srand(val);
        MemoryArena *a = NULL;
        // 创建一个容量为 256 个 Block 的 Arena 用于测试
        a = arena_create(256);

        // test1(a);
        // arena_clean(a); 

        // test2(a); printf("test2 pass\n");

        // 运行我们的高强度混合压力测试
        test3(a);

        arena_free(a);
    } 
}
