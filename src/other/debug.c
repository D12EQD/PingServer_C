#define _GNU_SOURCE
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "ds/linklist.h"
#include "other/global_time.h"
#include "other/debug.h"
#include "other/def.h"

uint64_t _ping_g_debug_flags = 0;
uint8_t _ping_g_debug_level = 0; 
List* _debug_statistics_list = NULL;
FILE* debug_log_fp = NULL;
//
static const char *debug_level_color(uint8_t level) {
    switch (level) {
        case LV_FATAL: return ANSI_BRIGHT_RED;   // 致命：亮红
        case LV_ERROR: return ANSI_RED;          // 错误：红
        case LV_WARN:  return ANSI_YELLOW;       // 警告：黄
        default:       return "";                // 其他：青
    }
    return "";
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

// file : 执行debug文件的文件名称
// line : 执行debug文件的文件具体行数
// flag : debug 过滤标志
// level : debug 等级
// fp : debug 日志输出的文件描述符
// format : 需要输出的格式，使用 printf 类似风格
void ping_debug(const char *file, size_t line, uint8_t level, uint64_t flag, FILE* fp, const char *format, ...) {
    if (!(_ping_g_debug_flags & flag)) {
        return;
    }
    if (!(_ping_g_debug_level & level)){
        return;
    }

    if (unlikely( fp == NULL )) fp = stdout;   
     
    
    if (file){
        if (line == (size_t)(-1))
            fprintf(fp, "[%s] ", file);
        else
            fprintf(fp, "[%s:%lu] ", file, line);
    }

    if (unlikely(level & LV_TIME)){
        fprintf(fp, " [time : %lf] ", global_get_time_ns() / 1e9);
    }

    if (_ping_g_debug_level & level) fprintf(fp, debug_level_color(level));
    va_list args;
    va_start(args, format);

    vfprintf(fp, format, args);
    va_end(args);

    if (_ping_g_debug_level & level) fprintf(fp, ANSI_RESET);
    fflush(fp); 
}
