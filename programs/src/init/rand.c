#include "rand.h"

#define DEFAULT_MZ 0x1234567890ABCDEF
#define DEFAULT_MW 0xC001C0DEC001C0DE

static uint64_t m_z = DEFAULT_MZ;
static uint64_t m_w = DEFAULT_MW;

void seed(uint64_t seed)
{
	m_z = DEFAULT_MZ;
	m_w = seed;
}

uint64_t rand()
{
	m_z = 36969 * (m_z & 0xFFFFFFFF) + (m_z >> 32);
	m_w = 18000 * (m_w & 0xFFFFFFFF) + (m_w >> 32);

	return (m_z << 32) + m_w;
}
