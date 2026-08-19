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
typedef uint32_t key_t;

typedef struct{
    uint32_t key;
    uint8_t status;
}hashTableEntry;

typedef struct{
    hashTableEntry *table;
    void * val_array; // 存放真实key数组 
    size_t val_size; // key - val 中 val的真实大小
    uint32_t size;
    uint32_t delete_count;
    uint32_t alive_count;
}hashTable;

// 获取hash->array[pos]的单元地址
#define hash_val_get(h, pos) ((char*)((h)->val_array) + (h)->val_size * pos)
#define hash_val_cmp(h, pos, val) (memcmp(hash_val_get(h, pos), val, h->val_size))
#define hash_val_cpy(h, pos, val) (memcpy(hash_val_get(h, pos), val, h->val_size))

hashTable* hash_create(size_t number, size_t val_size);
bool hash_insert(hashTable *h, key_t hash_key, void* val);
bool hash_delete(hashTable *h, hash_t hash);
void* hash_query(hashTable *h, hash_t hash);
void hash_free(hashTable *h);
void hash_table_print(hashTable *h);
