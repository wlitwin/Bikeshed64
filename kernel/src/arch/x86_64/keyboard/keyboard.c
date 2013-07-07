#include "keyboard.h"

#include "arch/x86_64/kprintf.h"
#include "arch/x86_64/textmode.h"
#include "arch/x86_64/serial.h"
#include "arch/x86_64/interrupts/apic.h"
#include "arch/x86_64/interrupts/interrupts.h"

static uint8_t scan_code_table[2][128] =
{
	{
/* 00-07 */	0,	'\033',	'1',	'2',	'3',	'4',	'5',	'6',
/* 08-0f */	'7',	'8',	'9',	'0',	'-',	'=',	'\b',	'\t',
/* 10-17 */	'q',	'w',	'e',	'r',	't',	'y',	'u',	'i',
/* 18-1f */	'o',	'p',	'[',	']',	'\n',	0,	'a',	's',
/* 20-27 */	'd',	'f',	'g',	'h',	'j',	'k',	'l',	';',
/* 28-2f */	'\'',	'`',	0,	'\\',	'z',	'x',	'c',	'v',
/* 30-37 */	'b',	'n',	'm',	',',	'.',	'/',	0,	'*',
/* 38-3f */	0,	' ',	0,	0,	0,	0,	0,	0,
/* 40-47 */	0,	0,	0,	0,	0,	0,	0,	'7',
/* 48-4f */	'8',	'9',	'-',	'4',	'5',	'6',	'+',	'1',
/* 50-57 */	'2',	'3',	'0',	'.',	0,	0,	0,	0,
/* 58-5f */	0,	0,	0,	0,	0,	0,	0,	0,
/* 60-67 */	0,	0,	0,	0,	0,	0,	0,	0,
/* 68-6f */	0,	0,	0,	0,	0,	0,	0,	0,
/* 70-77 */	0,	0,	0,	0,	0,	0,	0,	0,
/* 78-7f */	0,	0,	0,	0,	0,	0,	0,	0
	},
	{
/* 00-07 */	0,	'\033',	'!',	'@',	'#',	'$',	'%',	'^',
/* 08-0f */	'&',	'*',	'(',	')',	'_',	'+',	'\b',	'\t',
/* 10-17 */	'Q',	'W',	'E',	'R',	'T',	'Y',	'U',	'I',
/* 18-1f */	'O',	'P',	'{',	'}',	'\n',	0,	'A',	'S',
/* 20-27 */	'D',	'F',	'G',	'H',	'J',	'K',	'L',	':',
/* 28-2f */	'"',	'~',	0,	'|',	'Z',	'X',	'C',	'V',
/* 30-37 */	'B',	'N',	'M',	'<',	'>',	'?',	0,	'*',
/* 38-3f */	0,	' ',	0,	0,	0,	0,	0,	0,
/* 40-47 */	0,	0,	0,	0,	0,	0,	0,	'7',
/* 48-4f */	'8',	'9',	'-',	'4',	'5',	'6',	'+',	'1',
/* 50-57 */	'2',	'3',	'0',	'.',	0,	0,	0,	0,
/* 58-5f */	0,	0,	0,	0,	0,	0,	0,	0,
/* 60-67 */	0,	0,	0,	0,	0,	0,	0,	0,
/* 68-6f */	0,	0,	0,	0,	0,	0,	0,	0,
/* 70-77 */	0,	0,	0,	0,	0,	0,	0,	0,
/* 78-7f */	0,	0,	0,	0,	0,	0,	0,	0
	},
};

#define BUFFER_SIZE 256
static uint8_t buffer[BUFFER_SIZE];
static uint32_t next_index = 0;
static uint32_t last_index = 0;
static uint32_t buffer_size = 0;

uint8_t keyboard_char_available()
{
	return buffer_size > 0;
}

uint8_t keyboard_get_char()
{
	if (!keyboard_char_available())
	{
		return '\0';
	}

	uint8_t val = buffer[last_index];
	--buffer_size;
	++last_index;
	if (last_index >= BUFFER_SIZE)
	{
		last_index = 0;
	}

	return val;
}

static
void check_scan_code(uint8_t code)
{
	static uint8_t shiftdown = 0;
	static uint8_t ctrl_mask = 0xFF;

	switch (code)
	{
		case 0x2A:
		case 0x36:
			shiftdown = 1;
			break;
		case 0xAA:
		case 0xB6:
			shiftdown = 0;
			break;
		case 0x1D:
			ctrl_mask = 0x1F;
			break;
		case 0x9D:
			ctrl_mask = 0xFF;
		default:
		{
			// Handle on key-down
			if ((code & 0x80) == 0)
			{
				code = scan_code_table[shiftdown][code];
				if (code != 0)
				{
					buffer[next_index] = code;
					++buffer_size;

					++next_index;
					if (next_index >= BUFFER_SIZE)
					{
						next_index = 0;
						if (next_index == last_index)
						{
							last_index = (last_index + 1) % BUFFER_SIZE;
						}
					}

					// For debugging, will be removed
					if (code == '1')
					{
						page_up();
					}
					else if (code == '2')
					{
						page_down();
					}

				}
			}
		}
	}
}

#define KEYBOARD_DATA 0x60
#define KEYBOARD_STATUS 0x64
#define READY 0x1
#define EOT '\04'

static
void keyboard_handler(uint64_t vector, uint64_t code)
{
	UNUSED(vector);
	UNUSED(code);

	//kprintf("KEY\n");

	uint8_t scan_code = _inb(KEYBOARD_DATA);
	check_scan_code(scan_code);

//	apic_eoi();
	pic_acknowledge(vector);
}

void keyboard_init()
{
	interrupts_install_isr(33, keyboard_handler);
}
