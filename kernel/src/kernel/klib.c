#include "klib.h"

/*
 */
void memset(void* ptr, const uint8_t val, uint64_t size)
{
	uint8_t* p = (uint8_t*) ptr;
	while (size > 0)
	{
		*p = val;
		++p;
		--size;
	}
}

void* memcpy(void* dst, const void* src, uint64_t size)
{
	uint8_t* d = (uint8_t*) dst;
	const uint8_t* s = (const uint8_t*) src;

	while (size > 0)
	{
		*d = *s;
		++d;
		++s;
		--size;
	}

	return dst;
}
