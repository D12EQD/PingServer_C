/*
* 支持存放相同的key
* 不支持扩容操作
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "other/prime.h"

typedef uint32_t hash_t;

typedef struct{
    uint32_t key;
    uint8_t status;
}HashTableEntry;

typedef struct{
    HashTableEntry *table;
    void * val_array; // 存放真实key数组 
    int val_size; // key - val 中 val的真实大小
    uint32_t size;
    uint32_t delete_count;
    uint32_t alive_count;
    ping_alloc_t *alloc;
}HashTable;

// 获取hash->array[pos]的单元地址
#define hash_val_get(h, pos) ((char*)((h)->val_array) + (h)->val_size * pos)
#define hash_val_cmp(h, pos, val) (memcmp(hash_val_get(h, pos), val, h->val_size))
#define hash_val_cpy(h, pos, val) (memcpy(hash_val_get(h, pos), val, h->val_size))

HashTable* hash_init(size_t number, uint32_t val_size, ping_alloc_t *alloc);
bool hash_insert(HashTable *h, uint32_t hash_key, void* val);
bool hash_delete(HashTable *h, hash_t hash);
void* hash_query(HashTable *h, hash_t hash);
void hash_free(HashTable *h);
void hash_table_print(HashTable *h);
