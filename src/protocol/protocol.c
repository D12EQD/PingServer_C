#include "other/debug.h"
#include "other/def.h"
#include "protocol/protocol.h"
#include "protocol/http.h"

#define DEBUG_CONN(...) DEBUG(DEBUG_FLAG_CONNECTION, ##__VA_ARGS__)

/*
* 尝试读取conn中 read buf中的数据并且设置一个合适的协议处理上下文单元 protocol context
* returns : 错误码
*/
int connection_get_protocol_ctx(Connection *conn){
    if (get_http_protocol_handler_1_1()->on_read(conn) == 0){
        ((conn->protocol_handler)) = get_http_protocol_handler_1_1();
        return 0;
    }

    DEBUG_CONN("connction_get_protocol_ctx : ERROR_PROTO\n");
    return ERROR_PROTO;
}

void connection_clear_protocol(Connection *conn){
    protocolHandler * h = conn->protocol_handler;

    // TEST : h is NULL, so return ~~~
    return;

    h->on_close(conn);
    conn->protocol_handler = NULL;
}
