#pragma once
#include <stdint.h>
#include "ds/ping_arena.h"

typedef struct{
    uint32_t len;
    uint32_t cap;
    char data[];
}String;

/* This file was automatically generated.  Do not edit! */
void string_set_arena(MemoryArena *a);
bool string_append_char(String *str,char *s,size_t s_size);
String *string_recreate_from_arena(String *str,size_t new_size);
String *string_copy(String *str);
bool string_cmp_char(String *s1,char *s2,size_t s2_size);
bool string_is_prefix(String *s1,String *s2);
bool string_cmp(String *s1,String *s2);
void string_free(String *s);
String *string_create_from_arena(char *s, uint32_t size, size_t cap);

#define string_create(s) string_create_from_arena((char *)s, strlen(s), strlen(s)+ strlen(s) / 3 * 2) /* 创建string 并默认容量为 size 1.66倍数 */
#define string_recreate(s, size) string_recreate_from_arena(s, size)
