#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ds/bitmap.h"
#include "ds/ping_arena.h"

#define left_block(a) ((((Block*)a) - sizeof(Block))->prev)

void test1(MemoryArena *a){
    const size_t cost[] = {1, 2, 3, 4, 5, 6, 1000, 2000};
    size_t n = sizeof(cost) / sizeof(size_t);

    void *bef = NULL;
    
    for (size_t i = 0; i < n; i ++){
        if (bef == NULL){
            bef = arena_alloc(a, cost[i]);
            continue;
        }

        void *now = arena_alloc(a, cost[i]);
        if (now == NULL){
            printf("%lu alloc faild\n", i);
            continue;
        }
        
        printf("check %lu now, diff %lu\n", i, now - bef);

        for (int j = 0; j < 20; j ++){
            printf(" %d", BIT_GET(a->bitmap, j));
        }
        printf("\n");
        bef = now;
    }

    printf("test1 pass\n");
}

/**
 * @brief 
 * 大量碎片化内存分配
 * @param a 
 */
void test2(MemoryArena *a){
    int n = 4096;
    int m = 0;

    int *id = (int *)malloc(sizeof(int) * n);
    int *count = (int *)malloc(sizeof(int) * n);

    int sum = 0;
    // 1. 随机分配
    for (int i = 0 ; i < n; i ++){
        count[i] = rand() % 40;
        sum += count[i];
        if (sum > 1500){
            m = i;
            break;
        }
    }

    for (int i = 0; i < m; i ++){
        id[i] = i;
    }

    for (int i = 0; i < m / 2; i ++){
        int x = rand() % m;
        int y = rand() % m;

        int temp = id[x];
        id[x] = id[y];
        id[y] = temp;        
    }

    void **ptr = (void**)malloc(sizeof(void *) * m);

    // 分配随机大小
    for (int i = 0; i < m; i ++){
        ptr[i] = arena_alloc(a, count[i] * 4096);
    }

    // 随机释放
    for (int i = 0; i < m / 2; i ++){
        arena_recycle(a, ptr[id[i]]);
        printf("arena_recycle id[i] : %d\n", id[i]);
    }

    // 随机释放 + 分配同时进行
    for (int i = m / 2; i < m; i ++){
        arena_alloc(a, count[i] * 4096);
        arena_recycle(a, ptr[id[i]]);
    }

    free(ptr);
    free(id);
    free(count);
}

int main(){
    srand(time(0));
    MemoryArena *a = NULL;
    a = arena_create(0);

    // test1(a);
    // arena_clean(a); 
    test2(a); printf("test2 pass\n");
    arena_free(a);
}