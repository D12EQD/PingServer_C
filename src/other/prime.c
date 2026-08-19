#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "other/prime.h"

uint32_t *prime;
uint32_t prime_size = 0; 

void prime_init(void){
    uint32_t *st = (uint32_t *)malloc(sizeof(uint32_t) * MAX_PRIME_NUMBER);
    memset(st, 0, sizeof(uint32_t) * MAX_PRIME_NUMBER);

    int prime_count = 0; // prime_count为质数数量

    for (uint32_t i = 2; i < MAX_PRIME_NUMBER; i ++){
        if (st[i]) continue;
        prime_count ++;
        if (prime_count % 100 == 0) prime_size ++;
        for (int j = i * 2; j < MAX_PRIME_NUMBER; j += i){
            st[j] = 1;
        }
    }

    // 每100个质数中取一个数字 
    prime = (uint32_t*)malloc(prime_size * sizeof(uint32_t));
    prime_count = 0;

    unsigned int idx = 0;
    for (uint32_t i = 0; i < MAX_PRIME_NUMBER; i ++){
        if (!st[i]){
            prime_count ++;
            if (prime_count % 100 == 0) {
                prime[idx ++] = i; 
            }
            if (idx == prime_size){
                break;
            }
        }
    }

    free(st);
}

