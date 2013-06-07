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

