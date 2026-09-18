#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "net/connection.h"
#include "other/debug.h"
#include "net/tcp_server.h"

#define N 1000

int main(){
    DEBUG_FLAG_SET(DEBUG_FLAG_ALL);
    debug_log_fp = fopen("test/debug.txt", "w");

}
