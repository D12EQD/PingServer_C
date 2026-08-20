#ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 199309L
#endif 

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "ds/linklist.h"
#include "other/debug.h"

uint32_t _ping_g_debug_flags = 0;
linkList* _debug_statistics_list = NULL;

#define debug_statistics_ptr(i) ((debug_statistics_t *)(i))

// debug时间测量工具
struct timespec temp = {0};

uint32_t debug_gettime_sec(){
    clock_gettime(CLOCK_MONOTONIC, &temp);
    return temp.tv_sec;
}

void ping_debug(const char *file, uint32_t flag, const char *format, ...) {
    // 检查该标志是否启用
    if (!(_ping_g_debug_flags & flag)) {
        return;
    }
    
    // 输出文件名
    printf("[%s] ", file);
    
    // 输出格式化信息
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_statistics_list_init(){
    _debug_statistics_list = (linkList*)malloc(sizeof(linkList));
    link_list_init(_debug_statistics_list);
}

void debug_statistics_list_free(){
    link_list_free(_debug_statistics_list);
}

/*
* 输入一个需要记录的统计数据名称，返回一个可以读写的指针
*/
debug_statistics_t* debug_statistics_register(const char* name){
    debug_statistics_t *st = (debug_statistics_t *)malloc(sizeof(debug_statistics_t));
    st->first_ts = 0;
    st->last_ts = 0;
    st->count = 0;
    st->name = name;

    ASSERT(_debug_statistics_list != NULL);
    ASSERT(list_insert_back(_debug_statistics_list, st));
    return st;
}

/**
 * 将秒数格式化为固定宽度 "HH:MM:SS"（8字符）
 */
static inline void time_format(char *buf, size_t bufsize, uint64_t seconds) {
    snprintf(buf, bufsize, "%02d:%02d:%02d",
             (int)(seconds / 3600),
             (int)((seconds / 60) % 60),
             (int)(seconds % 60));
}


void debug_statistics_list_print() {
    uint64_t total_count = 0;
    int item_count = 0;
    
    printf("\n========== Debug Statistics Report ==========\n");
    // 列宽定义：Name 30字符，Count 15字符，First/Last Trigger 各15字符
    printf("%-30s %-15s %-15s %-15s\n", "Name", "Count", "First Trigger", "Last Trigger");
    printf("-------------------------------------------------------------------------------------------\n");
    
    if (!_debug_statistics_list || list_is_empty(_debug_statistics_list)) {
        printf("(Debug statistics list is empty)\n");
        printf("==============================================\n");
        return;
    }

    char first_time[16], last_time[16];

    for (linkListNode *i = _debug_statistics_list->begin; i; i = i->next) {
        debug_statistics_t *st = (debug_statistics_t *)i;
        if (st->count == 0) continue;
        time_format(first_time, sizeof(first_time), st->first_ts);
        time_format(last_time, sizeof(last_time), st->last_ts);
        
        printf("%-30s %-15lu %-15s %-15s\n",
               st->name ? st->name : "unnamed",
               st->count,
               first_time,
               last_time);
        
        total_count += st->count;
        item_count++;
    }
    
    if (item_count == 0) {
        printf("(No statistics recorded yet)\n");
    } else {
        printf("-------------------------------------------------------------------------------------------\n");
        printf("Total: %d items, %lu total triggers\n", item_count, total_count);
    }
    printf("==============================================\n");
}

void debug_statistics_trigger(debug_statistics_t *st) {
    if (!st) return;
    
    st->count++;
    if (st->first_ts == 0) {
        st->first_ts = debug_gettime_sec(); 
        st->last_ts = debug_gettime_sec(); 
    }else{
        st->last_ts = debug_gettime_sec(); 
    }
}
