#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ds/hash_table.h"

#include "other/debug.h"
#include "other/prime.h"

#define MAX_HASH_TABLE_SIZE MAX_PRIME_NUMBER

enum hashStatus{
    HASH_EMPTY,
    HASH_ALIVE,
    HASH_DELETE,
};
#define HASH_REHASH_THRESHOLD 0.33

static inline void hash_check(hashTable *h);
static inline bool _hash_insert_self(hashTable *h, hash_t hash_key, void* val, bool is_check);

void hash_table_print(hashTable *h){
    DEBUG(DEBUG_FLAG_HASH, "[hash_table] ========== hashTable Info ==========\n");
    DEBUG(DEBUG_FLAG_HASH, "[hash_table] Size:         %u\n", h->size);
    DEBUG(DEBUG_FLAG_HASH, "[hash_table] Alive Count:  %u\n", h->alive_count);
    DEBUG(DEBUG_FLAG_HASH, "[hash_table] Delete Count: %u\n", h->delete_count);
    DEBUG(DEBUG_FLAG_HASH, "[hash_table] Val Size:     %d\n", h->val_size);
}

/*
* hash 插入
* 将val的数据拷贝进入当前hash表管理的数据内容中
*/
static inline bool _hash_insert_self(hashTable *h, hash_t hash_key, void* val, bool is_check){
    uint64_t pos = hash_key;
    uint32_t temp;
    int first_deleted = -1;

    if (is_check) hash_check(h);

    for (uint64_t i = 0, len = h->size; i < len; i++){
        temp = (pos + i * i) % len;

        if (h->table[temp].status == HASH_ALIVE && h->table[temp].key == hash_key){
            if (hash_val_cmp(h, temp, val) == 0){
                DEBUG(DEBUG_FLAG_HASH, "error in _hash_insert_self for : same key\n");
                return false;  // 说明存在相同key，返回错误
            }
            return true;
        }

        // 记录遇到的第一个 DELETE 位置
        if (first_deleted == -1 && h->table[temp].status == HASH_DELETE){
            first_deleted = (int)temp;
        }

        // 遇到 EMPTY 说明探测链结束，确定表中无重复项
        if (h->table[temp].status == HASH_EMPTY){
            // 优先复用之前记录的 DELETE 位置，否则存入当前 EMPTY 位置
            uint32_t target = (first_deleted != -1) ? (uint32_t)first_deleted : temp;
            
            if (first_deleted != -1) {
                h->delete_count--;
            }

            h->table[target].status = HASH_ALIVE;
            h->table[target].key = hash_key;
            hash_val_cpy(h, target, val);
            h->alive_count++;
            return true;
        }
    }

    if (first_deleted != -1){
        h->table[first_deleted].status = HASH_ALIVE;
        h->table[first_deleted].key = hash_key;
        hash_val_cpy(h, first_deleted, val);
        h->delete_count--;
        h->alive_count++;
        return true;
    }

    return false;
}

// 假设 HASH_REHASH_THRESHOLD 为 0.75 (即 75% 负载因子)
// 我们可以用整数乘法替换除法： (alive + delete) / size > 0.75  <=>  (alive + delete) * 4 > size * 3
static inline void hash_check(hashTable *h) {
    uint32_t total_occupied = h->alive_count + h->delete_count;

    // 1. 快速判定分支：纯整数逻辑，Hot Path 极速退出
    if (total_occupied * 4 <= h->size * 3 || h->delete_count * 3 <= h->alive_count) {
        return;
    }

    DEBUG(DEBUG_FLAG_HASH, "rehashing\n");

    uint32_t h_size = h->size;
    uint32_t val_size = h->val_size;

    hashTableEntry *new_table = (hashTableEntry*)calloc(h_size, sizeof(hashTableEntry));
    
    void *new_val = malloc(h_size * val_size);

    if (!new_table || !new_val) {
        free(new_table);
        free(new_val);
        return;
    }

    hashTableEntry *old_table = h->table;
    void *old_val = h->val_array;

    h->table = new_table;
    h->val_array = new_val;
    h->delete_count = 0;
    h->alive_count = 0;

    const char *old_val_bytes = (const char*)old_val;
    for (uint32_t i = 0; i < h_size; i++) {
        if (old_table[i].status == HASH_ALIVE) {
            _hash_insert_self(h, old_table[i].key, (void*)(old_val_bytes + i * val_size), false);
        }
    }

    free(old_table);
    free(old_val);
}

// 需要指定val数据大小 哈希表默认扩充两倍
hashTable* hash_create(size_t number, size_t val_size){
    number *= 2; // 默认扩充两倍
    if (number >= MAX_HASH_TABLE_SIZE) return NULL;
    int l = -1, r = prime_size - 1;

    while (l + 1 != r){
        int mid = (l + r) >> 1;

        // printf("%d %d %d\n", l, r, mid);

        if (prime[mid] >= number){
            r = mid;
        }else{
            l = mid;
        }
    }

    size_t size = prime[r]; 
    if (size < number) return NULL;

    hashTable *h = (hashTable *)malloc(sizeof(hashTable));
    
    if (!h) return NULL;

    h->table = (hashTableEntry*)malloc(size * sizeof(hashTableEntry));
    memset(h->table, 0, size * sizeof(hashTableEntry));

    h->val_array = malloc(size * val_size);

    h->size = size;
    h->val_size = val_size;
    h->delete_count = 0;
    h->alive_count = 0;

    return h; 
}


/*
* 初始化大小，指定需要的hash table的元素数量
* return null means failed
*/

void hash_free(hashTable *h){
    free(h->table);
    free(h->val_array);
    free(h);
}


bool hash_insert(hashTable *h, key_t hash_key, void* val){
    return _hash_insert_self(h, hash_key, val, true);
}

// 查询所在地址 returns address or NULL means failed
void* hash_query(hashTable *h, hash_t hash){
    uint64_t pos = hash;
    uint32_t temp;

    for (uint64_t i = 0, len = h->size; i < len; i ++){
        temp = (pos + i * i) % len;
        if (h->table[temp].status == HASH_ALIVE && h->table[temp].key == hash){
            // DEBUG(DEBUG_FLAG_HASH, "query sucess | key %lu in %lu\n", hash ,temp);
            return hash_val_get(h, temp);
        }
        if (h->table[temp].status == HASH_EMPTY){
            return NULL;
        }
    }

    return NULL;
}


/*
不free其中内存，由用户管理
returns true or false
*/
bool hash_delete(hashTable *h, hash_t hash){
    hash_check(h);
    uint64_t pos = hash;
    uint32_t temp;

    for (uint32_t i = 0, len = h->size; i < len; i ++){
        temp = (pos + i * i) % len;
        if (h->table[temp].status == HASH_ALIVE && h->table[temp].key == hash){
            h->table[temp].status = HASH_DELETE;
            h->delete_count ++;
            return true;
        }
        if (h->table[temp].status == HASH_EMPTY){
            return false;
        }
    }

    return false;
}
