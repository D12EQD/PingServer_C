#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <wchar.h>

#include "ds/buffer.h"
#include "other/def.h"
#include "other/global_time.h"
#include "net/connection.h"
#include "protocol/protocol.h"

#include "other/debug.h"


#define CONNECTION_BUFFER_BLOCK_SIZE 3
#define CONNECTION_REQUEST_COUNT 64
#define DEBUG_CONN(...) DEBUG(DEBUG_FLAG_CONNECTION, ##__VA_ARGS__)


// create a connection and init it.
// conn should be alloc before use
void connection_create(Connection* conn, int fd, struct sockaddr_in * addr, MemoryArena* a){
    size_t alloc_size = 0;
    conn->read_buf = (Buffer *)arena_alloc_block(a, CONNECTION_BUFFER_BLOCK_SIZE, &alloc_size);
    conn->read_buf->cap = alloc_size;
    
    conn->send_buf  = (Buffer *)arena_alloc_block(a, CONNECTION_BUFFER_BLOCK_SIZE, &alloc_size);
    conn->read_buf->cap = alloc_size;
    
    conn->arena = a;
    conn->fd = fd;

    memcpy(&conn->addr, addr, sizeof(struct sockaddr_in));

    conn->last_activity = global_get_time();
    
    conn->max_request_count = CONNECTION_REQUEST_COUNT;
    conn->request_count = 0;

    buffer_clean(conn->read_buf);
    buffer_clean(conn->send_buf);

    conn->is_dead = false;
}

// conection receive data, use buffer 
int connection_recv(Connection* conn){
    DEBUG_CONN("conn recv data!\n");
    
    if (conn->max_request_count == conn->request_count){
        return ERROR_CONN_FULL;
    }

    conn->request_count ++;
    
    Buffer *buf = conn->read_buf;
    if (buf->len == buf->cap){
        return ERROR_BUFFER_EMPTY;
    }

    int n = recv(conn->fd, buf->data + buf->len, buf->cap - buf->len, 0);
    buf->len += n;

    if (n == EAGAIN || n == EWOULDBLOCK) return 0;
    if (n < 0) return n;

    conn->last_activity = global_get_time();
    return n;
}

// connection send data, could use other buffer to send data
// if buffer is NULL, use conn->buffer to send data
int connection_send(Connection* conn, Buffer* buffer){
    if (conn->max_request_count == conn->request_count){
        return ERROR_CONN_FULL;
    }

    conn->request_count ++;
    Buffer *buf = buffer ? buffer : conn->send_buf;

    // send len byte 
    int n = 0;

    n = send(conn->fd, buf->data + buf->idx, buf->len, 0);
    if (n <= 0){
        DEBUG_CONN("connection_send error: error number %d\n", n);
        return n;
    }

    buf->idx += n;
    conn->last_activity = global_get_time();
    return n;
}

// close connection and free buffer
void connection_close(Connection* conn){
    buffer_free_from_arena(conn->read_buf, conn->arena);
    buffer_free_from_arena(conn->send_buf, conn->arena);
    close(conn->fd);
    conn->is_dead = true;
}

// free connection 
// warning : ban on this project becasue we don't use malloc to create Connection
void connection_free(Connection* conn){
    connection_close(conn);
    free(conn);
}

