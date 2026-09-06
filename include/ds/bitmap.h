#pragma once

#include <stdint.h>
#include <stdlib.h>

#define BIT_SET(map, index)   ((map)[(index) >> 3] |=  (1U << ((index) & 7)))
#define BIT_UNSET(map, index) ((map)[(index) >> 3] &= ~(1U << ((index) & 7)))
#define BIT_GET(map, index)   (((map)[(index) >> 3] >> ((index) & 7)) & 1U)

void bitmap_set_range(uint8_t *map, size_t start, size_t end);
void bitmap_unset_range(uint8_t *map, size_t start, size_t end);