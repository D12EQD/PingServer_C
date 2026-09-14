#include <asm-generic/errno.h>
#include <stdio.h>
#include <stdnoreturn.h>


noreturn void test(){
    int val = 1;
    printf("%d", val);

    for (int i = 0; i < 10 ; i ++){
        printf("%d", i);
    }
}

int main(){
    for (int i = 0; i < 10; i ++){
        test();
    }
    return 0;
}
