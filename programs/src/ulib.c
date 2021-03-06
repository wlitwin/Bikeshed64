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

void set_priority(uint8_t priority)
{
	UNUSED(priority);

	__asm__ volatile ("movq $" SX(SYSCALL_SET_PRIO) ", %r10");
	__asm__ volatile ("int $" SX(SYSCALL_INT_VEC));
}

uint8_t key_available()
{
	register uint8_t retVal __asm__("rax");

	__asm__ volatile ("movq $" SX(SYSCALL_KEY_AVAIL) ", %r10");
	__asm__ volatile ("int $" SX(SYSCALL_INT_VEC) ::: "%rax");

	return retVal;
}

uint8_t get_key()
{
	register uint8_t retVal __asm__("rax");

	__asm__ volatile ("movq $" SX(SYSCALL_GET_KEY) ", %r10");
	__asm__ volatile ("int $" SX(SYSCALL_INT_VEC) ::: "%rax");

	return retVal;
}

uint8_t read_key(void)
{
	while (!key_available())
	{
		msleep(0);
	}

	return get_key();
}
