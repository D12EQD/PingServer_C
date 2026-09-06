#include <string.h>
#include <stdio.h>
#include "other/def.h"
#include "ds/bitmap.h"

// 批量置 1 (Set Range)
void bitmap_set_range(uint8_t *map, size_t start, size_t end) {
    size_t start_byte = start >> 3;
    size_t end_byte = end >> 3;

    printf("set range [%lu, %lu]\n", start, end);

    if (likely(start_byte == end_byte)) {
        // 边界在同一个字节内
        uint8_t mask = ((1U << ((end - start) + 1)) - 1) << (start & 7);
        map[start_byte] |= mask;
    } else {
        // 1. 处理首字节：保留低位，将从 start 开始到末尾的位设为 1
        map[start_byte] |= (0xFF << (start & 7));

        // 2. 处理中间的整字节：直接用 memset 批量赋 0xFF
        if (end_byte > start_byte + 1) {
            memset(&map[start_byte + 1], 0xFF, end_byte - start_byte - 1);
        }

        // 3. 处理尾字节：将从 0 到 end 的位设为 1，保留高位
        map[end_byte] |= ((1U << ((end & 7) + 1)) - 1);
    }
}

// 批量清 0 (Unset Range)
void bitmap_unset_range(uint8_t *map, size_t start, size_t end) {
    size_t start_byte = start >> 3;
    size_t end_byte = end >> 3;

    if (likely(start_byte == end_byte)) {
        // 边界在同一个字节内
        uint8_t mask = ~(((1U << ((end - start) + 1)) - 1) << (start & 7));
        map[start_byte] &= mask;
    } else {
        // 1. 处理首字节：保留 start 低位的原样，高位置 0
        map[start_byte] &= (1U << (start & 7)) - 1;

        // 2. 处理中间的整字节：直接用 memset 批量赋 0x00
        if (end_byte > start_byte + 1) {
            memset(&map[start_byte + 1], 0x00, end_byte - start_byte - 1);
        }

        // 3. 处理尾字节：保留 end 高位的原样，低位置 0
        map[end_byte] &= ~((1U << ((end & 7) + 1)) - 1);
    }
}