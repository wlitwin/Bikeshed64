#include "../inttypes.h"
#include "../ulib.h"

#include "vga.h"
#include "shell.h"

void main(void)
{
	set_priority(3);

	// Create the idle process
	Pid pid;
	if (fork(&pid) == SUCCESS)
	{
		if (pid == 0)
		{
			// Idle process does nothing
			while (1);
		}
		else
		{
			set_priority(0);
			// Run the shell
			shell_loop();
		}
	}
	else
	{
		text_mode_info.color.fg = FG_LIGHT_RED;
		text_mode_info.color.bg = BG_BLACK;
		text_mode_string("FAILED TO FORK!");
		while (1);
	}
}
