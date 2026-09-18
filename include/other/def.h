#pragma once

#ifndef min
    #define min(a, b) ((a) > (b) ? (b) : (a))
#endif

#ifndef max
    #define max(a, b) ((a) < (b) ? (b) : (a))
#endif

#define ERROR_BUFFER_FULL       -2 // 读/写缓冲区已满
#define ERROR_SYSTEM            -3 // 系统调用失败
#define ERROR_CONN_CLOSED       -4 // 对端已关闭连接（可选，因为 recv 返回0也可表示）
#define ERROR_CONN_FULL         -5 // 服务器最大连接已满，关闭
#define ERROR_BUFFER_EMPTY      -6 // 读写缓冲区空了 不能满足服务读取
#define ERROR_HTTP_PARSE        -8 // 协议错误
#define ERROR_HTTP_NEED_MORE    -9 // 需要更多数据
#define ERROR_TCP_CLOSE         -10 // tcp连接关闭
#define ERROR_TCP_TIME_OUT      -11 // tcp连接超时，服务器需要关闭
#define ERROR_MEM_FULL          -12 // arena分配内存达到上限

#define PROTO_HTTP_1_1    1
#define PROTO_HTTP_1_0    0

// 一些优化
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define ANSI_RESET          "\033[0m"
#define ANSI_RED            "\033[31m"
#define ANSI_GREEN          "\033[1;32m"
#define ANSI_YELLOW         "\033[33m"
#define ANSI_BLUE           "\033[34m"
#define ANSI_MAGENTA        "\033[35m"
#define ANSI_CYAN           "\033[36m"
#define ANSI_BRIGHT_RED     "\033[1;31m"
#define ANSI_BRIGHT_YELLOW  "\033[1;33m"
#define ANSI_BRIGHT_GREEN   "\033[1;32m"


enum tcp_proto_ret{
    TCP_PROTO_CLOSE,   // 协议要求服务做出 close
    TCP_PROTO_SEND,    // 协议要求服务做出 send
};

enum conn_send_ret {
    CONN_SEND_DONE,     // 全部发完
    CONN_SEND_PARTIAL,  // 发了一部分，还差，等 EPOLLOUT
    CONN_SEND_ERROR,    // 真错误（非 EAGAIN）
};

enum http_send_ret {
    HTTP_SEND_WAIT,     // 正在等待资源
    HTTP_SEND_HEAD,     // 正在传输 head
    HTTP_SEND_BODY,     // 正在传输 body
    HTTP_SEND_DONE,     // 传输完成
    HTTP_SEND_ERROR,    // error
};

// some function
#define str_check(s1, size_s1, s2) (strlen(s2) != size_s1 ? 0 : memcmp(s1, s2, size_s1) == 0) /* ture is ok */
