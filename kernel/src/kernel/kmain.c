#include "kernel/virt_memory/defs.h"
#include "kernel/interrupts/defs.h"

void kmain(void)
{
	/* Initialize the memory sub-system */
	virt_memory_init();

	/* Initialize the interupt sub-system */
	interrupts_init();

	while (1) {
		__asm__("hlt");
	}
}
