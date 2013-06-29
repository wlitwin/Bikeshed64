#include "../inttypes.h"
#include "../ulib.h"

#define VIDEO_ADDRESS 0xFFFF8000000B8000
#define MAX_X 80
#define MAX_Y 25

#define FG_BLACK 0x0
#define FG_BLUE  0x1
#define FG_GREEN 0x2
#define FG_CYAN  0x3
#define FG_RED   0x4
#define FG_MAGENTA 0x5
#define FG_BROWN 0x6
#define FG_WHITE 0x7
#define FG_GRAY 0x8
#define FG_LIGHT_BLUE 0x9
#define FG_LIGHT_GREEN 0xA
#define FG_LIGHT_CYAN 0xB
#define FG_LIGHT_RED 0xC
#define FG_LIGHT_MAGENTA 0xD
#define FG_LIGHT_YELLOW 0xE
#define FG_LIGHT_WHITE 0xF

#define BG_BLACK 0x0
#define BG_BLUE  0x1
#define BG_GREEN 0x2
#define BG_CYAN  0x3
#define BG_RED   0x4
#define BG_MAGENTA 0x5
#define BG_BROWN 0x6
#define BG_WHITE 0x7
#define BG_GRAY 0x8

typedef struct
{
	uint8_t fg : 4;
	uint8_t bg : 3;
	uint8_t blink : 1;
} color_t;

typedef struct
{
	char c;
	color_t color;
} video_cell;

video_cell* video = (video_cell*)VIDEO_ADDRESS;
uint16_t x;
uint16_t y;

color_t color = { FG_BLUE, BG_BLACK, 0 };

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

const char* str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1";
int count = 0;
void print_code()
{
	while(1)
	{
		++count;
		const int index = (count) % 27;
		char printit[2] = { str[index], '\0' };
		text_mode_string(printit);
		msleep(500);
//		__asm__ volatile("hlt");
	}
}

void main(void)
{
	const color_t colors[16] = 
	{
		{ FG_LIGHT_WHITE, BG_BLACK, 0 },
		{ FG_LIGHT_BLUE, BG_BLACK, 0 },
		{ FG_LIGHT_GREEN, BG_BLACK, 0 },
		{ FG_LIGHT_CYAN, BG_BLACK, 0 },
		{ FG_LIGHT_MAGENTA, BG_BLACK, 0 },
		{ FG_LIGHT_YELLOW, BG_BLACK, 0 },
		{ FG_GRAY, BG_BLACK, 0 },
		{ FG_BLUE, BG_BLACK, 0 },
		{ FG_RED, BG_BLACK, 0 },
		{ FG_GREEN, BG_BLACK, 0 },
		{ FG_CYAN, BG_BLACK, 0 },
		{ FG_BROWN, BG_BLACK, 0 },
		{ FG_MAGENTA, BG_BLACK, 0 },
		{ FG_BLACK, BG_WHITE, 0 },
		{ FG_LIGHT_RED, BG_BLACK, 0 },
		{ FG_WHITE, BG_CYAN, 0 },
	};

	/*Pid pid;
	if (fork(&pid) == SUCCESS)
	{
		if (pid == 0)
		{
			while (1) { __asm__ volatile("hlt"); }
		}
		else
		{
			x = 40;
			print_code();
		}
	}
	*/

	x = 40;

	int i = 0;
	for (; i < 16; ++i)
	{
		Pid pid;
		if (fork(&pid) == SUCCESS)
		{
			if (pid == 0)
			{
				if (i == 15)
				{
					while (1) { __asm__ volatile("hlt"); }
				}
				continue;
			}
			else
			{
				break;
			}
		}
		else
		{
			color.fg = FG_LIGHT_RED;
			color.bg = BG_BLACK;
			//color = colors[i];
			x = 60;
			y = 20;
			//text_mode_char('0' + i);
			text_mode_string("FAILED TO FORK!");
			while (1) { __asm__ volatile ("hlt"); }
		}
	}

	y += i;
	y = y % 25;
	color = colors[i];
	print_code();
}
