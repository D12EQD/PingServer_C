#include <asm-generic/errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ds/buffer.h"
#include "other/def.h"
#include "other/global_time.h"
#include "net/connection.h"
#include "protocol/protocol.h"

#include "other/debug.h"
#include "ds/arena.h"


#define CONNECTION_BUFFER_SIZE 4192
#define DEBUG_CONN(...) DEBUG(DEBUG_FLAG_CONNECTION, ##__VA_ARGS__)

void connection_print(connection_t* conn){
    DEBUG_CONN(
        "\n"
        "connection_t print : \n"
        "    ptr %p\n"
        "    read_buf %p\n"
        "    write_buf %p\n"
        "    arena %p\n"
    , conn, conn->read_buf, conn->write_buf, conn->arena);
}

/*
* args:
*  - fd: socket文件描述符
*  - addr: 地址
*  - fa : 上层分配的内存分配器，叫爸爸
* return:
*  - 成功: 返回指向新创建的connection_t结构体的指针，否则返回NULL
*/
connection_t* connection_create(int fd, struct sockaddr_in addr, Arena* fa){
    connection_t* conn = arena_alloc(fa, sizeof(connection_t));
    conn->arena = (Arena *) arena_alloc(fa, sizeof(Arena));
    memset(conn->arena, 0, sizeof(Arena));    
    
    conn->fd = fd;
    conn->addr = addr;
    conn->last_activity = global_get_time();
    conn->read_buf = buffer_create_from_arena(CONNECTION_BUFFER_SIZE, conn->arena); // 创建接收缓冲区
    conn->write_buf = buffer_create_from_arena(CONNECTION_BUFFER_SIZE, conn->arena); // 创建发送缓冲

    return conn;

// clean_and_return:
    if (conn) {
        arena_free(conn->arena);
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
    DEBUG_CONN("connection_recv start\n");


    if (buffer_writable(conn->read_buf) == 0){
        return ERROR_BUFFER_EMPTY;
    }
    int n = recv(conn->fd, conn->read_buf->data, buffer_writable(conn->read_buf), 0);

    conn->last_activity = global_get_time();
    
    if (n < 0){
        if (n != EAGAIN && n != EWOULDBLOCK){
            return 0;
        }
        return n;
    }
    
    buffer_write(conn->read_buf, n);
    DEBUG_CONN("connection_recv end\n");

    return n;
}

/*
* 向 TCP 套接字写入数据
* 若buffer为NULL，则读取conn中的write_buf发送该数据
*/
int connection_send(connection_t* conn, buffer_t* buffer){
    DEBUG_CONN("connection_send start\n");

    buffer_t *buf = buffer ? buffer : conn->write_buf;

    if (buf->len == 0){
        DEBUG_CONN("connection_send error: buffer full\n");
        return ERROR_BUFFER_FULL;
    }

    int n = send(conn->fd, buf->data, buf->len, 0);
    if (n <= 0){
        DEBUG_CONN("connection_send error: error number %d\n", n);
        return n;
    }

    DEBUG_CONN("connection_send end1\n");
    buffer_write(buf, n);
    conn->last_activity = global_get_time();
    DEBUG_CONN("connection_send end2\n");

    return n;
}

void connection_close(connection_t* conn){
    conn->last_activity = global_get_time();
    conn->protocol_ctx = NULL;
    close(conn->fd);
}

void connection_free(connection_t* conn){
    connection_close(conn);
    arena_free(conn->arena); // 不用管buffer了 统一由这个分配
    free(conn);
}

bool connection_is_timeout(connection_t *conn, uint64_t timeout_ms){
    return (global_get_time() - conn->last_activity) >= timeout_ms;
}

void connection_reset(connection_t *conn){
    connection_close(conn);
    buffer_reset(conn->read_buf);
    buffer_reset(conn->write_buf);
    arena_reset(conn->arena);
}

void connection_init(connection_t* conn, int fd, struct sockaddr_in addr, Arena* fa){
    if (!(conn->arena)){
        conn->arena = (Arena *)arena_alloc(fa, sizeof(Arena));
        memset(conn->arena, 0, sizeof(Arena));
    }
    
    conn->read_buf = buffer_create_from_arena(CONNECTION_BUFFER_SIZE, conn->arena); // 接收缓冲
    conn->write_buf = buffer_create_from_arena(CONNECTION_BUFFER_SIZE, conn->arena); // 发送缓冲

    conn->fd = fd;
    conn->addr = addr;
    conn->last_activity = global_get_time();
    conn->protocol_ctx = (protocolContext *) arena_alloc(conn->arena, sizeof(protocolContext));
    ((protocolContext *)(conn->protocol_ctx)) -> protocol_temp = NULL;
    ((protocolContext *)(conn->protocol_ctx)) -> handler = NULL;

    return;
}
