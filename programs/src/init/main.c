#include "../inttypes.h"
#include "../ulib.h"

#define VIDEO_ADDRESS 0xFFFF8000000B8000
#define MAX_X 80
#define MAX_Y 25

typedef struct
{
	char c;
	uint8_t color;
} video_cell;

video_cell* video = (video_cell*)VIDEO_ADDRESS;
uint16_t x;
uint16_t y;

uint8_t color = 0x9;

void clear_screen()
{
	unsigned char* video = (unsigned char*)VIDEO_ADDRESS;
	for (int i = 0; i < 80*25; ++i)
	{
		video[i] = 0;
	}
}

void text_mode_char(const char c)
{
	switch (c)
	{
		case '\r':
		case '\n':
			{
				x = 0;
				++y;
				if (y >= MAX_Y)
					y = 0;
			}
			return;
	}

	video[y*80+x].c = c;
	video[y*80+x].color = color;

	++x;
	if (x >= MAX_X)
	{
		x = 0;
		++y;
		if (y >= MAX_Y)
		{
			y = 0;
		}
	}
}

void text_mode_string(const char* str)
{
	while (*str != '\0')
	{
		text_mode_char(*str);
		++str;
	}
}

void main(void)
{
	Pid pid;
	if (fork(&pid) == SUCCESS)
	{
		const char* str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1";
		int count = 0;
		if (pid == 0)
		{
			// parent
			while(1)
			{
				++count;
				const int index = (count+x) % 27;
				char printit[2] = { str[index], '\0' };
				text_mode_string(printit);
				__asm__ volatile("hlt");
			}
		}
		else
		{
			color = 0xC;
			++y;
			// child
			while(1)
			{
				++count;
				const int index = (count+x) % 27;
				char printit[2] = { str[index], '\0' };
				text_mode_string(printit);
				__asm__ volatile("hlt");
			}
			return;
		}
	}

	x = y = 0;
	while (1)
	{
		text_mode_string("Hello World! ");
	}
}
