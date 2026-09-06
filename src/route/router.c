#include "net/connection.h"
#include "ds/hash_table.h"
#include <string.h>
#include <stdlib.h>

#include "other/debug.h"
#include "other/def.h"
#include "route/router.h"

#define DEBUG_ROUTER(...) DEBUG(DEBUG_FLAG_ROUTER, ##__VA_ARGS__)

typedef struct{
    char *path;
    routerFunction func;
}router_t;

void main_web(Connection *conn, void* return_val);

router_t router_table[] = {
    {"/", main_web}  
};

static inline bool router_check(char *router, size_t router_len){
    return false;
}

routerFunction get_router_function(const char *path, size_t path_len, const char *method, size_t method_len){
    
    return NULL;
}

void main_web(Connection *conn, void* return_val){
    
}
