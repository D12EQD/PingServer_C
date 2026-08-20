#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <assert.h>

#define PING_DS_IMPLEMENT
#define PING_DS_DEBUG_ENABLE // open debug mode

#define PORT 8080
#define DATA_SIZE 1000000
#define BUFFER_SIZE DATA_SIZE

char app_ack[64];
char ack_buf[1024];
char buffer[BUFFER_SIZE];

void* server_main(void *){
    int sock = -1;
    
    struct sockaddr_in local, client_addr;
    uint32_t addrlen = sizeof(client_addr);
    
    int bytesRead = 0;
    int res = 0;

    // 1. 创建 socket 
    // 创建一个基于 IPv4、使用 TCP 协议的数据通道
    sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    // 2. 允许端口重用（方便调试，避免 "Address already in use"）
    // 该操作是：别管那些还在 TIME_WAIT 上一次链接了，告诉操作系统需要直接使用
    int opt = 1;
    assert(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) >= 0);

    // 3. 绑定到本机 8080 端口
    // 将上面空的套接字绑定具体网络地址信息
    local.sin_family = AF_INET; // 要求传输ip协议
    local.sin_addr.s_addr = INADDR_ANY; // 监听本机所有网卡
    local.sin_port = htons(PORT); // 指定端口

    assert(bind(sock, (struct sockaddr *)&local, sizeof(local)) >= 0);

    // 4. 监听队列设置
    if (listen(sock, 1) < 0) {
        printf("server listen error\n");
        goto close_and_clean;
    }
    printf("服务端: 监听端口 %d\n", PORT);

    // 5. 接受客户端连接 分配了一个新的文件fd用来表示客户端
    int client = accept(sock, (struct sockaddr *)&client_addr, &addrlen);
    assert(client >= 0);

    printf("服务端: 客户端已连接 (来自 %s)\n", inet_ntoa(client_addr.sin_addr));

    assert(write(client, "220 Welcome\r\n", 13));
    printf("服务端: 已发送欢迎语 (13 字节)\n");

    while(1) {
        res = read(client, buffer, BUFFER_SIZE);
        if (res < 0) {
            printf("server read failed\n");
            goto close_and_clean;
        }

        if (res == 0) break; // 收到 FIN（正常关闭）
        bytesRead += res;
        printf("服务端: 累计收到 %d 字节\n", bytesRead);
    }

    printf("服务端: 总共收到 %d 字节\n", bytesRead);

    // 向应用层确认已经收到的数据
    int len = snprintf(app_ack, sizeof(app_ack), "ACK: Received %d bytes\r\n", bytesRead);
    
    if (write(client, app_ack, len) < 0) {
        printf("server send app ack error\n");
    } else {
        printf("服务端: 已发送应用层确认\n");
    }

close_and_clean:
    if (client > 0) close(client); // 触发服务端的FIN 
    if (sock > 0) close(sock);
    return NULL;
}

int main() {
    pthread_t server;

    pthread_create(&server, NULL, server_main, NULL);
    pthread_join(server, NULL);

    return 0;
}