#include "net/connection.h"
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ds/buffer.h"
#include "other/def.h"
#include "other/global_time.h"

#include "ds/arena.h"

#ifndef ARENA_IMPLEMENTATION
    #define ARENA_IMPLEMENTATION
#endif
#include "ds/arena.h"

connection_t* connection_create(int fd, struct sockaddr_in addr, Arena* a);
int connection_recv(connection_t* conn);
int connection_send(connection_t* conn);
void connection_close(connection_t* conn);

#define CONNECTION_BUFFER_SIZE 2048

/*
* args:
*  - fd: socket文件描述符
*  - addr: 地址
*  - alloc: 内存分配函数指针，用于分配connection_t结构
* return:
*  - 成功: 返回指向新创建的connection_t结构体的指针，否则返回NULL
*/
connection_t* connection_create(int fd, struct sockaddr_in addr, Arena* a){
    connection_t* conn = arena_alloc(a, sizeof(connection_t));
    conn->fd = fd;
    conn->state = CONN_IDLE;
    conn->addr = addr;
    conn->last_activity = global_get_time();
    conn->read_buf = buffer_create_from_arena(CONNECTION_BUFFER_SIZE, a); // 创建接收缓冲区
    conn->write_buf = buffer_create_from_arena(CONNECTION_BUFFER_SIZE, a); // 创建发送缓冲

    return conn;
clean_and_return:

    if (conn) {
        buffer_free(conn->read_buf);
        buffer_free(conn->write_buf);
        free(conn);
    }

    return NULL;
}

/*
* 从 TCP 套接字读取数据
*/
int connection_recv(connection_t* conn){
    if (conn->state = CONN_CLOSING) return 0;

    buffer_t *buf = conn->read_buf;
    conn->state = CONN_READING;

    if (buffer_read_cap(buf) == 0){
        return ERROR_BUFFER_FULL;
    }

    int n = recv(conn->fd, buf, buffer_read_cap(buf), 0);
    if (n <= 0) return n;
    
    buffer_has_written(buf, n);
    conn->last_activity = global_get_time();
    return n;
}

/*
* 向 TCP 套接字写入数据
*/
int connection_send(connection_t* conn){
    if (conn->state = CONN_CLOSING) return 0;
    
    buffer_t *buf = conn->write_buf;
    conn->state = CONN_WRITING;

    if (buffer_write_cap(buf) == 0){
        return ERROR_BUFFER_FULL;
    }

    int n = send(conn->fd, buf, buffer_write_cap(buf), 0);
    if (n <= 0) return n;

    buffer_has_written(buf, n);
    conn->last_activity = global_get_time();
    return n;
}

void connection_close(connection_t* conn){
    conn->state = CONN_CLOSING;
    conn->last_activity = global_get_time();
    close(conn->fd);
}

bool connection_is_timeout(connection_t *conn, uint64_t timeout_ms){
    return (global_get_time() - conn->last_activity) >= timeout_ms;
}