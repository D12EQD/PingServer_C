#define _GNU_SOURCE

#include "other/debug.h"
#include "other/def.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "ds/str.h"
#include "protocol/http.h"
#include "ds/buffer.h"
#include "net/connection.h"

#define DEBUG_TEST(...) DEBUG(DEBUG_FLAG_ALL, ##__VA_ARGS__)

Connection conn;
MemoryArena *a;

static const char http_packet[] =
    "GET / HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "User-Agent: C-Client/1.0\r\n"
    "Accept: */*\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Authorization: Bearer token123\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: keep-alive\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 0\r\n"
    "Cookie: sessionid=abc123; theme=dark\r\n"
    "Server-Name: nginx\r\n"
    "\r\n";

// 注册的文件描述符
static int g_peer_fd = -1;

static inline int http_init(Connection* conn){
    HttpContext * ctx = arena_alloc_ref_block(conn->arena, sizeof(HttpContext));

    if (ctx == NULL){
        return ERROR_MEM_FULL;
    }
    memset(ctx, 0 ,sizeof(HttpContext));
    conn->protocol_ctx = ctx;
    return 0;
}

static void init(void) {
    a = arena_create(0);
    if (!a) {
        perror("arena_create");
        exit(1);
    }

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        perror("socketpair");
        exit(1);
    }
    g_peer_fd = sv[1];

    struct sockaddr_in sock_add;
    memset(&sock_add, 0, sizeof(sock_add));
    sock_add.sin_family      = AF_INET;
    sock_add.sin_port        = htons(80);
    sock_add.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  /* 127.0.0.1 */
    connection_create(&conn, sv[0], &sock_add, a);
    Buffer *buf = conn.read_buf;
    size_t n = strlen(http_packet);

    conn.protocol_handler = get_http_protocol_handler_1_1();

    if (n > buf->cap) {
        exit(1);
    }

    memcpy(buf->data, http_packet, n);
    buf->len += n;
}

int main(void) {
    DEBUG_FLAG_SET(-1);
    DEBUG_LEVEL_SET(-1);
    
    init();
    string_set_arena(a);
    
    protocolHandler *handler = conn.protocol_handler;
    if (!handler || !handler->on_read) {
        DEBUG_TEST(LV_INFO, "no on_read handler\n");
        return 1;
    }

    int ret = handler->on_read(&conn);
    DEBUG_TEST(LV_INFO, "on_read returned %d\n", ret);
    ret = handler->on_process(&conn);
    DEBUG_TEST(LV_INFO, "on_process returned %d\n", ret);
    handler->on_write(&conn);
    return 0;
}
