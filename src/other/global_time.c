#define _GNU_SOURCE

#include <time.h>
#include <stdint.h>

struct timespec g_time;

// 单位为ms
uint64_t global_get_time() {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    
    // 计算经过的时间（毫秒）
    uint64_t elapsed_ms = (uint64_t)(current_time.tv_sec - g_time.tv_sec) * 1000;
    elapsed_ms += (current_time.tv_nsec - g_time.tv_nsec) / 1000000;
    
    return elapsed_ms;
}