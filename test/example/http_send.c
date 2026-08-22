#define _GNU_SOURCE
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

void send_response(int socket_fd, const char *body, size_t body_len) {
    static const char header_prefix[] = 
        "HTTP/1.1 200 OK\r\n"
        "Server: MyZeroCopyServer/1.0\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "Content-Length: ";
    
    // 1. 将缓冲区放大（例如 64 字节），绝对防止溢出
    char length_buf[64];
    int length_len = snprintf(length_buf, sizeof(length_buf), "%zu\r\n\r\n", body_len);

    struct iovec iov[3];
    
    iov[0].iov_base = (void*)header_prefix;
    iov[0].iov_len  = sizeof(header_prefix) - 1;

    iov[1].iov_base = (void*)length_buf;
    iov[1].iov_len  = length_len;

    iov[2].iov_base = (void*)body;     
    iov[2].iov_len  = body_len;

    // 2. 检查 writev 返回值
    ssize_t nwritten = writev(socket_fd, iov, 3);
    if (nwritten < 0) {
        perror("writev failed");
    }
}

int main() {
    const char *message = "fuck you\n";
    send_response(1, message, strlen(message));
    return 0;
}