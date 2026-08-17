#include <stdio.h>

#include "core/ping_alloc.h"
#include "ds/linklist.h"
#include "other/debug.h"

void __attribute__((constructor)) all_init(void){
    debug_statistics_list_init();
    ping_alloc_debug_init();
}

void __attribute__((destructor)) all_free(void){
    debug_statistics_list_free();
}

#define N 1000

int main(){
    DEBUG_FLAG_SET(DEBUG_FLAG_ALL);

    debug_statistics_t *st = debug_statistics_register("testing");
    
    for (int i = 0; i < 100000; i ++){
        if (i % 1000 == 0) debug_statistics_trigger(st);
    }

    ping_alloc_t *p = ping_alloc_create();
    
    int *a = ping_alloc(p, sizeof(int) * N);

    printf("a is %p\n", a);
    printf("a[N - 1] is %p\n", &a[N - 1]);

    for (int i = 0; i < N; i ++){
        a[i] = i;
        printf("%d\n", a[i]);
    }

    debug_statistics_list_print();
    ping_alloc_free(p);

    return 0;
}