#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <sys/sendfile.h>
#include <stdint.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <wchar.h>

#include "ds/buffer.h"
#include "other/def.h"
#include "other/global_time.h"
#include "other/debug.h"

#include "net/connection.h"

#include "protocol/protocol.h"
#define DEBUG_CONN(...) DEBUG(DEBUG_FLAG_CONNECTION, ##__VA_ARGS__)

static int connection_id = 0;

// create a connection and init it.
// conn should be alloc before use
// return 0 means OK, or return ERROR_MEM_FULL
int connection_create(Connection* conn, int fd, struct sockaddr_in * addr, MemoryArena* a){
    size_t alloc_size = 0;
    conn->read_buf = (Buffer *)arena_alloc_block(a, CONNECTION_BUFFER_BLOCK_SIZE, &alloc_size);
    if (conn->read_buf == NULL){
        return ERROR_MEM_FULL;
    }
    conn->read_buf->cap = alloc_size;
    conn->send_buf  = (Buffer *)arena_alloc_block(a, CONNECTION_BUFFER_BLOCK_SIZE, &alloc_size);
    if (conn->send_buf == NULL){
        arena_recycle(a, conn->read_buf);
        return ERROR_MEM_FULL;
    }
    conn->send_buf->cap = alloc_size;
    
    conn->arena = a;
    conn->fd = fd;
    conn->conn_id = connection_id ++;
    memcpy(&conn->addr, addr, sizeof(struct sockaddr_in));
    conn->last_activity = global_get_time();
    conn->max_request_count = CONNECTION_REQUEST_COUNT;
    conn->request_count = 0;

    conn->protocol_ctx = NULL;
    conn->protocol_handler = NULL;
    connection_get_protocol_ctx(conn);


    buffer_clean(conn->read_buf);
    buffer_clean(conn->send_buf);
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    DEBUG_CONN(LV_INFO, "read buffer cap %lu | send buffer cap %lu\n", conn->read_buf->cap, conn->send_buf->cap);
    conn->conn_st = CONN_ST_IDLE;
    return 0;
}

// conection receive data, use buffer 
// return 0 or ERROR_BUFFER_FULL or ERROR_CONN_FULL or the error in errno.h
int connection_recv(Connection* conn){
    DEBUG_CONN(LV_INFO, "conn recv data\n");
    
    if (conn->max_request_count == conn->request_count){
        return ERROR_CONN_FULL;
    }
    conn->conn_st = CONN_ST_READING; 
    conn->request_count ++;
    
    Buffer *buf = conn->read_buf;
    if (buf->len == buf->cap){
        return ERROR_BUFFER_FULL;
    }
    
    bool flag = false;
    int n = 0;

    while (!flag){
        int n = recv(conn->fd, buf->data + buf->len, buf->cap - buf->len, 0);

        if (likely(n > 0)) buf->len += n;
        else flag = true;
    }

    conn->last_activity = global_get_time();
    // 不设置 conn_st ，不知道是否需要再次读入，由后续事件再修改是否为空闲或者其他状态
    return n;
}

// connection send data, could use other buffer to send data
// flag 是 send 函数使用的 flag，见 man 2 send
// return conn_send_ret 
int connection_send(Connection* conn, Buffer* buffer, int flag){
    if (conn->max_request_count == conn->request_count) return ERROR_CONN_FULL;
    conn->conn_st = CONN_ST_WRITING;

    Buffer *buf = buffer ? buffer : conn->send_buf;
    conn->last_activity = global_get_time();
    conn->request_count ++;

    DEBUG_CONN(LV_INFO, "send_buf: cap=%u len=%u idx=%u\n", buf->cap, buf->len, buf->idx);
    DEBUG_CONN(LV_INFO, "send_buf data: |%.*s|\n", (int)buf->len, buf->data);

    int n = 0;
    size_t remaining = buf->len - buf->idx;
    ASSERT((int)(buf->len - buf->idx) > 0);
    while ((n = send(conn->fd, buf->data + buf->idx, remaining, flag)) > 0) {
        buf->idx += n;
        remaining -= n;
    }

    if (n < 0){
        DEBUG_CONN(LV_WARN, "connection_send error: error number %d\n", n);
        if (unlikely(n != EAGAIN)){
            conn->conn_st = CONN_ST_IDLE; // 错误 设置为 idle
            return CONN_SEND_ERROR;
        }
    }else{
        if (buf->idx == buf->len){
            DEBUG_CONN(LV_INFO, "connection_send success\n"); 
            conn->conn_st = CONN_ST_IDLE; // 完成 设置为 idle
            return CONN_SEND_DONE;
        }
    }

    DEBUG_CONN(LV_WARN, "connection_send failed for some reasons\n");
    conn->conn_st = CONN_ST_WAITING; // 部分 设置为 waiting
    return CONN_SEND_PARTIAL;
}

// conn为Connection 连接 ，fd为文件描述符
int connection_send_file(Connection* conn, int fd){
    if (conn->max_request_count == conn->request_count) return ERROR_CONN_FULL;
    conn->conn_st = CONN_ST_WRITING;
    conn->last_activity = global_get_time();
    conn->request_count ++;
    
    // TODO : 完成 sendfile

    conn->conn_st = CONN_ST_WAITING; // 部分 设置为 waiting
    return CONN_SEND_PARTIAL;   
}

// close connection and free buffer
void connection_close(Connection* conn){
    buffer_free_from_arena(conn->read_buf, conn->arena);
    buffer_free_from_arena(conn->send_buf, conn->arena);
    close(conn->fd);
    conn->conn_st = CONN_ST_CLOSING;
}

// free connection 
// warning : ban on this project becasue we don't use malloc to create Connection
void connection_free(Connection* conn){
    connection_close(conn);
    free(conn);
}

int connection_send_fd(Connection * conn, int fd, off_t *offset_ptr, uint32_t len){
    int remain = len - *offset_ptr;
    while (remain > 0) { 
        // 文件不会超过2^30所以不处理
        ssize_t n = sendfile(conn->fd, fd, offset_ptr, remain);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN){
                return CONN_SEND_PARTIAL;
            }
            return CONN_SEND_ERROR; 
        }
        if (n == 0) break;
        *offset_ptr += n;
        remain -= n;
    }
    return CONN_SEND_DONE;
}
