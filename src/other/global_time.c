#define _GNU_SOURCE

#include <time.h>
#include <stdint.h>

struct timespec g_time;
struct timespec current_time;

void global_time_init(){
    clock_gettime(CLOCK_MONOTONIC, &g_time);
}

uint64_t global_get_time() {
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    
    uint64_t elapsed_ms = (uint64_t)(current_time.tv_sec - g_time.tv_sec) * 1000;
    elapsed_ms += (current_time.tv_nsec - g_time.tv_nsec) / 1000000;
    
    return elapsed_ms;
}