#ifndef __KLIB_H__
#define __KLIB_H__

#include "inttypes.h"

static inline __attribute__((always_inline))
uint64_t min(uint64_t val, uint64_t min, uint64_t max)
{
	if (val < min) return min;
	if (val > max) return max;
	return val;
}

void memclr(void* ptr, uint64_t size);

void memset(void* ptr, uint8_t val, uint64_t size);

void* memcpy(void* dst, const void* src, uint64_t size);

#endif
