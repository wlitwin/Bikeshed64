#ifndef __X86_64_SUPPORT_H__
#define __X86_64_SUPPORT_H__

#include "inttypes.h"

static inline __attribute__((always_inline))
uint8_t _inb(uint16_t port)
{
	uint8_t retVal;
	__asm__ volatile("inb %1, %0" : "=a"(retVal) : "d"(port));
	return retVal;
}

static inline __attribute__((always_inline))
uint16_t _inw(uint16_t port)
{
	uint16_t retVal;
	__asm__ volatile("inw %1, %0" : "=a"(retVal) : "d"(port));
	return retVal;
}

static inline __attribute__((always_inline))
uint32_t _inl(uint16_t port)
{
	uint32_t retVal;
	__asm__ volatile("inl %1, %0" : "=a"(retVal) : "d"(port));
	return retVal;
}

static inline __attribute__((always_inline))
void _outb(uint16_t port, uint8_t val)
{
	__asm__ volatile("outb %0, %1" : : "a"(val), "d"(port));
}

static inline __attribute__((always_inline))
void _outw(uint16_t port, uint16_t val)
{
	__asm__ volatile("outw %0, %1" : : "a"(val), "d"(port));
}

static inline __attribute__((always_inline))
void _outl(uint16_t port, uint32_t val)
{
	__asm__ volatile("outl %0, %1" : : "a"(val), "d"(port));
}

#endif
