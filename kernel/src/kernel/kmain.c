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

void print_string(const char* str)
{
	struct video_cell* video = (struct video_cell*) 0xB8000;

	int x = 0;
	int y = 0;

	while (*str)
	{
		video[y*80+x].c = *str;
		video[y*80+x].color = 0x9;

		++str;
		++x;
		if (x >= 80)
		{
			x = 0;
			++y;
			if (y >= 25)
			{
				y = 0;
			}
		}
	}
}

void kmain(void)
{
	clear_screen();
	print_string("Welcome to 64-bit Bikeshed!");

	/* Initialize the memory sub-system */
	virt_memory_init();

	while (1) {
		__asm__("hlt");
	}
}
