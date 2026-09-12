#include <stdio.h>
#include <sys/epoll.h>

typedef struct temp{
    int val;
    int val2;
    int val3;
}T;

T array[100];

int main(){
    EBADF;
    printf("%lu", (void *)(&array[10]) - (void *)array);
    return 0;
}
