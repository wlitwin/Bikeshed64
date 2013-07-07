#include "vga.h"

video_cell* video = (video_cell*)VIDEO_ADDRESS;

text_mode_t text_mode_info =
{
	.x = 0,
	.y = 0,
	.color = { .fg = FG_LIGHT_BLUE, .bg = BG_BLACK, .blink = 0 },
};

void clear_screen()
{
	uint16_t* video = (uint16_t*)VIDEO_ADDRESS;
	for (int i = 0; i < MAX_X*MAX_Y; ++i)
	{
		video[i] = 0;
	}
}

uint16_t text_get_x()
{
	return text_mode_info.x;
}

uint16_t text_get_y()
{
	return text_mode_info.y;
}

void text_set_pos(uint8_t x, uint8_t y)
{
	text_mode_info.x = x;
	text_mode_info.y = y;
}

void text_mode_char(const char c)
{
	switch (c)
	{
		case '\r':
		case '\n':
			{
				text_mode_info.x = 0;
				++text_mode_info.y;
				if (text_mode_info.y >= MAX_Y)
					text_mode_info.y = 0;
			}
			return;
		case '\b':
			{
				if (text_mode_info.x == 0)
				{
					text_mode_info.x = MAX_X;
					if (text_mode_info.y > 0)
					{
						--text_mode_info.y;
					}
				}
				--text_mode_info.x;

				const uint32_t offset = text_mode_info.y*MAX_X + text_mode_info.x;
				video[offset].c = ' ';
				video[offset].color = text_mode_info.color;
			}
			return;
	}

	const uint32_t offset = text_mode_info.y*MAX_X + text_mode_info.x;
	video[offset].c = c;
	video[offset].color = text_mode_info.color;

	++text_mode_info.x;
	if (text_mode_info.x >= MAX_X)
	{
		text_mode_info.x = 0;
		++text_mode_info.y;
		if (text_mode_info.y >= MAX_Y)
		{
			text_mode_info.y = 0;
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

