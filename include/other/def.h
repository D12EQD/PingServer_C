#pragma once
#ifndef min
    #define min(a, b) ((a) > (b) ? (b) : (a))
#endif

#ifndef max
    #define max(a, b) ((a) < (b) ? (b) : (a))
#endif

#define ERROR_BUFFER_FULL  -2  // 读/写缓冲区已满
#define ERROR_SYSTEM       -3  // 系统调用失败
#define ERROR_CONN_CLOSED  -4  // 对端已关闭连接（可选，因为 recv 返回0也可表示）
#define ERROR_CONN_FULL    -5  // 服务器最大连接已满，关闭
#define ERROR_BUFFER_EMPTY -6  // 读写缓冲区空了