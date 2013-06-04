#include "arch/x86_64/serial.h"
#include "arch/x86_64/support.h"

#define SERIAL_PORT_A 0x3F8

void init_serial_debug()
{
	_outb(SERIAL_PORT_A + 1, 0x00);
	_outb(SERIAL_PORT_A + 3, 0x80);
	_outb(SERIAL_PORT_A + 0, 0x03);
	_outb(SERIAL_PORT_A + 1, 0x00);
	_outb(SERIAL_PORT_A + 3, 0x03);
	_outb(SERIAL_PORT_A + 2, 0xC7);
	_outb(SERIAL_PORT_A + 4, 0x08);
}

int serial_transmit_empty()
{
	return _inb(SERIAL_PORT_A + 5) & 0x20;
}

void serial_char(char c)
{
	while (serial_transmit_empty() == 0); 
	_outb(SERIAL_PORT_A, c);
}
