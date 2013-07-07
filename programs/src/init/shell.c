#include "shell.h"

#include "vga.h"
#include "rand.h"
#include "string.h"
#include "tetris.h"
#include "../ulib.h"

#define COMMAND_SIZE 256

#define STR_SIZE 44
static const char* str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%&*";
static char command[COMMAND_SIZE];
static uint32_t command_length = 0;
static color_t text_color = { .fg = FG_LIGHT_BLUE, .bg = BG_BLACK, .blink = 0 };

static void fancy_display_char(const char c)
{
	if (c != ' ')
	{
		const uint32_t offset = text_get_y()*MAX_X + text_get_x();
		for (uint32_t i = 0; i < 10; ++i)
		{
			const uint32_t str_offset = rand() % STR_SIZE;
			video[offset].c = str[str_offset];
			video[offset].color = text_color;
			msleep(10);
		}
	}

	text_mode_char(c);
}

static void fancy_display_string(const char* string)
{
	while (*string != 0)
	{
		fancy_display_char(*string);
		++string;
	}
}

static void display_banner()
{
	fancy_display_string("Welcome to Bikeshed64!");
}

static void write_string(const char* str)
{
	while (*str)
	{
		char c = *str;
		++str;

		text_mode_char(c);
/*		if (y == 0)
		{
			y = 1;
		}
		*/
	}
}

static void read_command()
{
	text_mode_string(">");

	command_length = 0;
	uint8_t key = read_key();
	while (command_length < COMMAND_SIZE-1 && key != '\n')
	{
		// Echo the character
		text_mode_char(key);

		if (key == '\b')
		{
			if (command_length > 0)
			{
				--command_length;
			}
		}
		else
		{
			// Save the character
			command[command_length] = key;
			++command_length;
		}

		key = read_key();
	}
	command[command_length] = 0;

	write_string("\n");
}

static void check_command()
{
	if (streq("help", command))
	{
		write_string("Commands:\n");
		write_string(" help - Display this text\n");
		write_string(" tetris - Play tetris\n");
	}
	else if (streq("tetris", command))
	{
		tetris();
	}
	else
	{
		write_string("Unrecognized command\n");
	}
}

void shell_loop()
{
	tetris();
	return;
	text_set_pos(0, 0);
	clear_screen();
	display_banner();


	text_set_pos(0, 1);
	while (1)
	{
		read_command();
		check_command();
	}
}
