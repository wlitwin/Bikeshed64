#include "ulib.h"

#include "safety.h"
#include "kernel/syscalls/syscalls.h"

Status fork(Pid* pid)
{
	// It's not really unused, the kernel modifies it
	UNUSED(pid);

	register Status retVal __asm__("rax");
	
	__asm__ volatile ("movq $" SX(SYSCALL_FORK) ", %r10");
	__asm__ volatile ("int $" SX(SYSCALL_INT_VEC) ::: "%rax");

	return retVal;
}

void msleep(time_t ms)
{
	UNUSED(ms);

	__asm__ volatile ("movq $" SX(SYSCALL_MSLEEP) ", %r10");
	__asm__ volatile ("int $" SX(SYSCALL_INT_VEC));
}

void exit()
{
	__asm__ volatile ("movq $" SX(SYSCALL_EXIT) ", %r10");
	__asm__ volatile ("int $" SX(SYSCALL_INT_VEC));
}
