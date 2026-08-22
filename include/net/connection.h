#pragma once
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// #include "protocol/protocol.h"
#include "ds/buffer.h"
#include "ds/arena.h"

typedef enum { 
    CONN_IDLE, 
    CONN_READING, 
    CONN_WRITING, 
    CONN_CLOSING
} enumConnState;


typedef struct {
    buffer_t* read_buf;  // 接收缓冲
    buffer_t* write_buf;  // 发送缓冲
    Arena *arena; // 由arena创建的内存分配器具
    struct sockaddr_in addr; // 客户端地址
    int fd; // Socket文件描述符
    enumConnState state;
    void* protocol_ctx;    
    time_t last_activity; // 最后活动时间（超时检测）
} connection_t;

connection_t* connection_create(int fd, struct sockaddr_in addr, Arena* a);
void connection_init(connection_t* conn, int fd, struct sockaddr_in addr, Arena* fa);
int connection_recv(connection_t* conn);
int connection_send(connection_t* conn);
void connection_close(connection_t* conn);
void connection_free(connection_t* conn);
void connection_reset(connection_t* conn);

