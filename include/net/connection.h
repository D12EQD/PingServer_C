#pragma once
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ds/buffer.h"

typedef enum { 
    CONN_IDLE, 
    CONN_READING, 
    CONN_WRITING, 
    CONN_CLOSING
} enumConnState;


typedef struct {
    MemoryArena *arena; // 由arena创建的内存分配器具
    struct sockaddr_in addr; // 客户端地址
    int fd; // Socket文件描述符
    void* protocol_handler;    
    time_t last_activity; // 最后活动时间（超时检测）
    Buffer* read_buf;  // 接受缓冲
    Buffer* send_buf;  // 发送缓冲
    int request_count; // 当前已经处理的请求数量
    int max_request_count; // 规定的最大的请求数量
} Connection;

void connection_create(Connection* conn, int fd, struct sockaddr_in addr, MemoryArena* a);
int connection_recv(Connection* conn);
int connection_send(Connection* conn, Buffer* buffer);
void connection_close(Connection* conn);
void connection_free(Connection* conn);

