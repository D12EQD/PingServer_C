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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "ds/linklist.h"

// 调试flag 需要新debug在此添加
#define DEBUG_FLAG_ALL      0xFFFFFFFF
#define DEBUG_FLAG_ALLOC    0x00000001
#define DEBUG_FLAG_HASH     0x00000002

// debug状态统计-次数统计工具
struct debug_statistics {
    struct link_list_base_node node;
    const char *name; // 状态名称
    uint64_t count; // 触发次数
    uint64_t first_ts; // 第一次触发time
    uint64_t last_ts;  // 最后一次触发time
};
typedef struct debug_statistics debug_statistics_t;

// debug.h 全局区域
extern uint32_t _ping_g_debug_flags;

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

void ping_debug(const char *file, uint32_t flag, const char *format, ...);
void debug_statistics_list_init();
void debug_statistics_list_print();
debug_statistics_t* debug_statistics_register(const char* name);
void debug_statistics_trigger(debug_statistics_t *st);
void debug_statistics_list_free();

#ifdef PINGNET_DEBUG_ENABLE
    // DEBUG 宏：自动带上文件名
    #define DEBUG(flag, format, ...) \
        ping_debug(__FILENAME__, flag, format, ##__VA_ARGS__)
    
    // 控制调试标志
    #define DEBUG_FLAG_SET(val)   (_ping_g_debug_flags |= (val))
    #define DEBUG_FLAG_UNSET(val) (_ping_g_debug_flags &= ~(val))
    #define DEBUG_FLAG_IS_SET(val) ((_ping_g_debug_flags & (val)) != 0)
    
    // 条件调试：只在 flag 启用时输出
    #define DEBUG_IF(flag, format, ...) \
        do { \
            if (DEBUG_FLAG_IS_SET(flag)) { \
                DEBUG(flag, format, ##__VA_ARGS__); \
            } \
        } while(0)
        
    #define ASSERT(x) assert(x)
#else
    // 禁用调试时，所有宏都是空操作
    #define DEBUG(flag, format, ...) ((void)0)
    #define DEBUG_FLAG_SET(val) ((void)0)
    #define DEBUG_FLAG_UNSET(val) ((void)0)
    #define DEBUG_FLAG_IS_SET(val) (0)
    #define DEBUG_IF(flag, format, ...) ((void)0)
    #define ASSERT(x) x
#endif