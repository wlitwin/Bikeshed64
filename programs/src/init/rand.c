#include "rand.h"

#define DEFAULT_MZ 0x12345678
#define DEFAULT_MW 0xC001C0DE

static uint32_t next = 0xC001C0DE;

void seed(uint64_t seed)
{
	next = (uint32_t)seed;
}

static uint32_t random32()
{
	next = next * 1103515245 + 12345;
	return (uint32_t)(next / 65536) % 32768;
}

uint64_t rand()
{
	return random32();
}
