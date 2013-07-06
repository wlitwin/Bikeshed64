#include "kernel/elf/elf.h"
#include "kernel/kprintf.h"
#include "kernel/sound/defs.h"
#include "kernel/timer/defs.h"
#include "kernel/alloc/alloc.h"
#include "kernel/keyboard/defs.h"
#include "kernel/interrupts/defs.h"
#include "kernel/virt_memory/defs.h"
#include "kernel/syscalls/syscalls.h"
#include "kernel/scheduler/scheduler.h"

void kmain(void)
{
	/* Initialize the memory sub-system */
	virt_memory_init();

	/* Initialize the kernel's memory allocators */
	alloc_init();

	/* Initialize the interupt sub-system */
	interrupts_init();

	/* Intialize the keyboard */
	keyboard_init();

	/* Initialize the scheduler */
	scheduler_init();

	/* Initialize the timer */
//	timer_init();

	/* Initialize the system calls */
	syscalls_init();

	/* Initialize the sound driver */
	sound_init();

	/* Setup the init process */
	//create_init_process();

	/* Wait forever, should not happen */
	while (1) {
		__asm__ volatile ("sti");
		__asm__ volatile ("hlt");
	}
}
