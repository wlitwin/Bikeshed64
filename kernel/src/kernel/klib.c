#include "klib.h"

void memclr(void* ptr, uint64_t size)
{
	uint8_t* p8 = (uint8_t*)ptr;
	while (((uint64_t)p8 & 0x7) != 0)
	{
		*p8++ = 0;
		--size;
	}

	uint64_t* p64 = (uint64_t*)p8;
	while (size >= 8)
	{
		*p64++ = 0;
		size -= 8;
	}

	if (size > 0)
	{
		uint8_t* p = (uint8_t*) p64;
		while (size > 0)
		{
			*p++ = 0;
			--size;
		}
	}
}

void memset(void* ptr, const uint8_t val, uint64_t size)
{
	uint8_t* p8 = (uint8_t*)ptr;
	while (((uint64_t)p8 & 0x7) != 0)
	{
		*p8++ = val;
		--size;
	}

	uint64_t* p64 = (uint64_t*)p8;
	uint64_t value = (val << 8) | val;
	value |= value << 16;
	value |= value << 32;
	while (size >= 8)
	{
		*p64++ = value;
		size -= 8;
	}

	if (size > 0)
	{
		uint8_t* p = (uint8_t*) p64;
		while (size > 0)
		{
			*p++ = val;
			--size;
		}
	}
}

void* memcpy(void* dst, const void* src, uint64_t size)
{
	uint8_t* d8 = (uint8_t*)dst;
	const uint8_t* s8 = (const uint8_t*)src;
	while (((uint64_t)d8 & 0x7) != 0)
	{
		*d8++ = *s8++;
		--size;
	}

	uint64_t* d64 = (uint64_t*)d8;
	const uint64_t* s64 = (const uint64_t*)s8;
	while (size >= 8)
	{
		*d64++ = *s64++;
		size -= 8;
	}

	if (size > 0)
	{
		uint8_t* d8 = (uint8_t*)d64;
		const uint8_t* s8 = (const uint8_t*)s64;
		while (size > 0)
		{
			*d8++ = *s8++;
			--size;
		}
	}

	return dst;
}
