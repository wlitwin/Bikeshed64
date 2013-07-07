#ifndef __VGA_H__
#define __VGA_H__

#include "../inttypes.h"

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
	uint16_t x;
	uint16_t y;
	color_t color;
} text_mode_t;

typedef struct
{
	char c;
	color_t color;
} video_cell;

extern video_cell* video;
extern text_mode_t text_mode_info;

void clear_screen(void);
void text_mode_char(const char c);
void text_mode_string(const char* str);
void text_set_pos(uint8_t x, uint8_t y);
uint16_t text_get_x(void);
uint16_t text_get_y(void);

#endif
