#include "other/def.h"
#include "protocol/protocol.h"
#include "protocol/http.h"
#include "ds/arena.h"

/* 
* 将连接绑定对应的协议，temp为暂存内存，保存协议相关信息，不同协议有不同的解释方式
*/
void connection_set_protocol(connection_t *conn, protocolHandler *handler, void *temp){
    protocolContext* ctx = conn->protocol_ctx;
    ctx->handler = handler;
    ctx->protocol_temp = temp; 
}

/*
* 尝试读取conn中 read buf中的数据并且设置一个合适的协议处理上下文单元 protocol context
* returns : 错误码
*/
int connction_get_protocol_ctx(connection_t *conn){
    if (get_http_protocol_handler()->on_read(conn) == 0){
        ((protocolContext*)(conn->protocol_ctx))->handler = get_http_protocol_handler();
        return 0;
    }

    return ERROR_PROTO;
}

/*
* 调用协议的clean函数并且解除conn->protocol_ctx的协议绑定
*/
void connection_clear_protocol(connection_t *conn){
    protocolContext *ctx = conn->protocol_ctx;
    if (ctx->handler && ctx->handler->on_close) {
        ctx->handler->on_close(conn);
    }
    conn->protocol_ctx = NULL;
}