#include "textmode.h"
#include "inttypes.h"

#include "arch/x86_64/virt_memory/physical.h"

#define MAX_X 80
#define MAX_Y 25

// 20 screens of text or 500 lines
#define NUM_PAGES (100)
#define NUM_LINES (MAX_Y*NUM_PAGES)
static uint8_t text[MAX_X*NUM_LINES];
static uint32_t page_offset = 0;

typedef struct
{
	char c;
	uint8_t color;
} video_cell;

#define VIDEO_ADDRESS (0xB8000+KERNEL_BASE)

static video_cell* video = (video_cell*)VIDEO_ADDRESS;

static uint16_t write_x;
static uint16_t write_y;

void text_mode_reset()
{
	write_x = write_y = 0;
}

void init_text_mode(void)
{
	write_x = write_y = 0;
}

void clear_screen()
{
	uint16_t* video = (uint16_t*)VIDEO_ADDRESS;
	for (int i = 0; i < MAX_X*MAX_Y; ++i)
	{
		video[i] = 0;
	}
}

static void display_page(uint32_t page_offset)
{
	clear_screen();
	uint32_t text_y = page_offset*MAX_Y;
	for (uint32_t disp_y = 0; disp_y < MAX_Y; ++disp_y, ++text_y)
	{
		for (uint32_t disp_x = 0; disp_x < MAX_X; ++disp_x)
		{
			const uint32_t text_offset = text_y*MAX_X + disp_x;
			const uint32_t offset = disp_y*MAX_X + disp_x;
			video[offset].c = text[text_offset];
			video[offset].color = 0x9;
		}
	}
}

void page_up()
{
	if (page_offset > 0)
	{
		--page_offset;
	}

	display_page(page_offset);
}

void page_down()
{
	const uint32_t max_page_offset = write_y / MAX_Y;
	if (page_offset < max_page_offset)
	{
		++page_offset;
	}

	display_page(page_offset);
}

void text_mode_char(char c)
{
	// Scroll to bottom
	const uint32_t current_page = write_y / MAX_Y;
	if (current_page != page_offset)
	{
		page_offset = current_page;
		clear_screen();
	}

	switch (c)
	{
		case '\r':
		case '\n':
			{
				++write_y;
				if (write_y >= NUM_LINES)
				{
					write_y = 0;
				}
				write_x = 0;
			}
			break;
		case '\b':
			{
				if (write_x == 0)
				{
					write_x = MAX_X;
					if (write_y > 0)
					{
						--write_y;
					}
				}
				--write_x;

				// Clear this spot
				text[write_y*MAX_X + write_x] = ' ';

				const uint32_t offset = (write_y%MAX_Y)*MAX_X + write_x;
				video[offset].c = ' ';
				video[offset].color = 0x9;
			}
			break;
		default:
			{
				text[write_y*MAX_X + write_x] = c;

				const uint32_t offset = (write_y%MAX_Y)*MAX_X + write_x;
				video[offset].c = c;
				video[offset].color = 0x9;
				
				++write_x;
				if (write_x >= MAX_X)
				{
					write_x = 0;
					++write_y;
					if (write_y >= NUM_LINES)
					{
						write_y = 0;
					}
				}
			}
			break;
	}
}
