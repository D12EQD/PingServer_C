#include "ds/ping_arena.h"
#include "other/debug.h"
#include "ds/str.h"
#include <string.h>

static MemoryArena * arena_g;

void string_set_arena(MemoryArena* a){
    arena_g = a;
}

// size 为 字符串 s 的大小 ， a 为内存管理池，cap为指定大小
String *string_create_from_arena(char *s, uint32_t size, size_t cap){
    String* res = arena_alloc_ref_block(arena_g, cap + sizeof(uint32_t) * 2);
    if (!res) return NULL;

    res->len = size;
    res->cap = cap;
    memcpy(res->data, s, size);
    return res;
}

void string_free(String *s){
    arena_recycle_ref(arena_g, s); 
}

// 判断是否完全相同 true 表示相同 false 不同
bool string_cmp(String* s1, String* s2){
    if (s1->len != s2->len) return false;
    return memcmp(s1->data, s2->data, s1->len) == 0;
}

// 判断是否是s1是s2的前缀
bool string_is_prefix(String * s1, String* s2){
    if (s1->len > s2->len) return false;
    return memcmp(s1->data, s2->data, s1->len) == 0;
}

// 判断是否完全相同 true 表示相同 false 不同 
bool string_cmp_char(String *s1, char *s2, size_t s2_size){
    if (s1->len != s2_size) return false;
    return memcmp(s1->data, s2, s1->len) == 0;
}
String *string_copy(String *str){
    String* res = arena_alloc_ref_block(arena_g, str->cap + sizeof(uint32_t) * 2);
    if (!res) return NULL;
    res->len = str->len;
    res->cap = str->cap;
    memcpy(res->data, str->data, res->len);
    return res;
}

String *string_recreate_from_arena(String * str, size_t new_size){
    String * res = arena_alloc_ref_block(arena_g, new_size);
    if (!res) return NULL;
    memcpy(res->data, str->data, str->len); 
    res->len = str->len;
    res->cap = new_size;
    arena_recycle_ref(arena_g, str);
    return res;
}

bool string_append_char(String *str, char *s, size_t s_size){
    if (str->cap - str->len < s_size) return false;
    memcpy(str->data + str->len - (str->data[str->len - 1] == 0), s, s_size);
    str->len += s_size;
    return true; 
}


