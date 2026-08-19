#pragma once
#ifndef min
    #define min(a, b) ((a) > (b) ? (b) : (a))
#endif

#ifndef max
    #define max(a, b) ((a) < (b) ? (b) : (a))
#endif

#define ERROR_BUFFER_FULL  -2   // 读/写缓冲区已满（无法继续接收或发送）
#define ERROR_SYSTEM       -3   // 系统调用失败（具体原因见 errno）
#define ERROR_CONN_CLOSED  -4   // 对端已关闭连接（可选，因为 recv 返回0也可表示）