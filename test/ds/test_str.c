#include <stdio.h>
#include "ds/str.h"
#include "ds/ping_arena.h"

int main(){
    MemoryArena* a = arena_create(0);
    string_set_arena(a);

    String* str = string_create("Hello World\n");
    printf("%s", str->data);

    if (!string_append_char(str, "123\n", sizeof("123\n"))){
        printf("append failed\n");
    }
    
    string_free(str);
    printf("%s", str->data);
}
