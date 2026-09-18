/*  
## usage:

#define PINGNET_DEBUG_ENABLE


### funcion 1: 
- 1. debug all
DEBUG_FLAG_SET(DEBUG_FLAG_ALL)

- 2. just let mac and ip layer to debug
DEBUG_FLAG_SET(DEBUG_FLAG_IPV4 | DEBUG_FLAG_MAC)

- 3. use DEBUG macro
DEBUG(DEBUG_FLAG_IPV4, "Packet received, len=%d\n", len);

### funcion 2:
- debug_statistics
for example:


*/

#pragma once  

#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include "ds/linklist.h"

// 调试flag 需要新debug在此添加
#define DEBUG_FLAG_ALL          0xFFFFFFFF
#define DEBUG_FLAG_ALLOC        0x00000001
#define DEBUG_FLAG_HASH         0x00000002
#define DEBUG_FLAG_TCPSERVER    0x00000004
#define DEBUG_FLAG_CONNECTION   0x00000008
#define DEBUG_FLAG_HTTP         0x00000010
#define DEBUG_FLAG_BUFFER       0x00000020
#define DEBUG_FLAG_ROUTER       0x00000040
#define DEBUG_FLAG_EVENT        0x00000080

// DEBUG 等级，每个等级占一个 bit
#define LV_FATAL    (1u << 0)
#define LV_ERROR    (1u << 1)
#define LV_WARN     (1u << 2)
#define LV_INFO     (1u << 3)
#define LV_TIME     (1u << 7) 
#define LV_ALL      0xFFu 


// debug状态统计-次数统计工具
struct debug_statistics {
    struct ListNode_s node;
    const char *name; // 状态名称
    uint64_t count; // 触发次数
    uint64_t first_ts; // 第一次触发time
    uint64_t last_ts;  // 最后一次触发time
};
typedef struct debug_statistics debug_statistics_t;

// debug.h 全局区域
extern uint64_t _ping_g_debug_flags;
extern FILE* debug_log_fp;
extern uint8_t _ping_g_debug_level;

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

void ping_debug(const char *file, size_t line, uint8_t level, uint64_t flag, FILE* fp, const char *format, ...);

#ifdef PINGNET_DEBUG_ENABLE
    // DEBUG 宏：自动带上文件名
    #ifdef PINGNET_DEBUG_LINE 
        #define DEBUG(flag, level, format, ...) \
            ping_debug(__FILE__, __LINE__, level, flag, debug_log_fp, format, ##__VA_ARGS__)
        
        #define DEBUG_CHAR(flag, level, format, ...) \
            ping_debug(NULL, __LINE__, level, flag, debug_log_fp, format, ##__VA_ARGS__)
    #else
        #define DEBUG(flag, level, format, ...) \
            ping_debug(__FILE__, -1, level, flag, debug_log_fp, format, ##__VA_ARGS__)
        
        #define DEBUG_CHAR(flag, level, format, ...) \
            ping_debug(NULL, -1, level, flag, debug_log_fp, format, ##__VA_ARGS__)
    #endif
    // 控制调试标志
    #define DEBUG_FLAG_SET(val)   (_ping_g_debug_flags |= (val))
    #define DEBUG_FLAG_UNSET(val) (_ping_g_debug_flags &= ~(val))
    #define DEBUG_FLAG_IS_SET(val) ((_ping_g_debug_flags & (val)) != 0)

    #define DEBUG_LEVEL_SET(val)   (_ping_g_debug_level |= (val))
    #define DEBUG_LEVEL_UNSET(val) (_ping_g_debug_level &= ~(val))
    #define DEBUG_LEVEL_IS_SET(val) ((_ping_g_debug_level & (val)) != 0)


    // 条件调试：只在 flag 启用时输出
    #define DEBUG_IF(flag, format, ...) \
        do { \
            if (DEBUG_FLAG_IS_SET(flag)) { \
                DEBUG(flag, format, ##__VA_ARGS__); \
            } \
        } while(0)
        
    #define ASSERT(x) \
        do{ \
            if (!( x )){ \
                ping_debug(__FILE__, __LINE__, LV_ERROR, DEBUG_FLAG_ALL, debug_log_fp, "ASSERT failed! Error number is %d\n", errno); \
                exit(1); \
            } \
        } while(0);
#else
    // 禁用调试时，所有宏都是空操作
    #define DEBUG(flag, format, ...) ((void)0)
    #define DEBUG_CHAR(flag, format, ...) ((void)0)
    #define DEBUG_FLAG_SET(val) ((void)0)
    #define DEBUG_FLAG_UNSET(val) ((void)0)
    #define DEBUG_FLAG_IS_SET(val) (0)

    #define DEBUG_LEVEL_SET(val) 0;
    #define DEBUG_LEVEL_UNSET(val) 0;
    #define DEBUG_LEVEL_IS_SET(val) 0;
    
    #define ASSERT(x) ((void)(x))
    
#endif
