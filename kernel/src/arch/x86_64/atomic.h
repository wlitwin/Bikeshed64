#ifndef __X86_64_ATOMIC_H__
#define __X86_64_ATOMIC_H__

// Credit to:
// http://www.mohawksoft.org/?q=node/78

static inline __attribute__((always_inline))
void atomic_inc(volatile int* num)
{
	__asm__ volatile ("lock incl %0" : "=m"(*num));
}

static inline __attribute__((always_inline))
void atomic_dec(volatile int* num)
{
	__asm__ volatile ("lock decl %0" : "=m"(*num));
}

#endif
