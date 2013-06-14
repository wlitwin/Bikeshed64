#ifndef __X86_64_CPUID_H__
#define __X86_64_CPUID_H__

#include "inttypes.h"

// Adapted from: http://wiki.osdev.org/CPUID

static inline __attribute__((always_inline))
void cpuid(uint32_t code, uint32_t* eax, uint32_t* edx)
{
	__asm__ volatile ("cpuid" : 
					  "=a"(*eax), "=d"(*edx) : // Outputs
					  "a"(code) : // Inputs
					  "ecx","ebx");	// Clobbered registers
}

static inline __attribute__((always_inline))
void writemsr(uint32_t msr_reg, uint32_t eax, uint32_t edx)
{
	__asm__ volatile ("wrmsr" : :
						"c"(msr_reg),
						"a"(eax),
						"d"(edx) :
						);
}

static inline __attribute__((always_inline))
void readmsr(uint32_t msr_reg, uint32_t* eax, uint32_t* edx)
{
	__asm__ volatile ("rdmsr" :
						"=a"(*eax), "=d"(*edx) : // Outputs
						"c"(msr_reg) :); // Inputs
}

#endif
