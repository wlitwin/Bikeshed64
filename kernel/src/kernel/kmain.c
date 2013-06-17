#include "kernel/virt_memory/defs.h"
#include "kernel/interrupts/defs.h"
#include "kernel/scheduler/scheduler.h"
#include "kernel/alloc/alloc.h"

#include "safety.h"

void kmain(void)
{
	/* Initialize the memory sub-system */
	virt_memory_init();

	/* Initialize the kernel's memory allocators */
	alloc_init();

	/* Initialize the interupt sub-system */
	interrupts_init();

	/* Initialize the scheduler */
	scheduler_init();

	while (1) {
		__asm__("hlt");
	}
}
