#include "arch/x86_64/kprintf.h"

void panic(const char* message)
{
	kprintf("%s - PANIC  \n", message);
	__asm__("cli");
	while (1)
	{
		__asm__("hlt");
	}
}
