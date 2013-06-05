#include "kernel/virt_memory/defs.h"

struct video_cell
{
	char c;
	unsigned char color;
};

void clear_screen()
{
	unsigned char* video = (unsigned char*)0xB8000;
	for (int i = 0; i < 80*25; ++i)
	{
		video[i] = 0;
	}
}

void kmain(void)
{
	clear_screen();

	/* Initialize the memory sub-system */
	virt_memory_init();

	while (1) {
		__asm__("hlt");
	}
}
