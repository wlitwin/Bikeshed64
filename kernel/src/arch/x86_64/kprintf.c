#include <stdarg.h>
#include "inttypes.h"
#include "panic.h"
//#include "arch/x86_64/serial.h"
#include "arch/x86_64/textmode.h"

#define BUFFER_LEN 21

static void     _kprintf(const char* fmt, va_list ap);
static void     padstr(const char* str, int64_t len, int64_t width, uint8_t leftadjust, char padchar);
static int64_t  convert_hexidecimal(char buf[static BUFFER_LEN], uint64_t value);
static int64_t  convert_decimal_u(char buf[static BUFFER_LEN], uint64_t value);
static int64_t  convert_decimal(char buf[static BUFFER_LEN], int64_t value);
static int64_t  convert_octal(char buf[static BUFFER_LEN], uint64_t value);
static uint64_t string_length(const char* str);

void kprintf(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	_kprintf(format, ap);
	va_end(ap);
}

static void write_char(char c)
{
	text_mode_char(c);
}

static void _kprintf(const char* fmt, va_list ap)
{
	char buffer[BUFFER_LEN];


	while (*fmt)
	{
		for (; *fmt && *fmt != '%'; ++fmt)
		{
			write_char(*fmt);
		}

		if (!*fmt)
		{
			return;
		}

		uint8_t leftadjust = 0;
		char padchar = ' ';
		int64_t width = 0;

		char ch = *(++fmt);
		if (ch == '-') {
			leftadjust = 1;
			ch = *(++fmt);
		}

		if (ch == '0') {
			padchar = '0';
			ch = *(++fmt);
		}

		while (ch != '\0' && ch >= '0' && ch <= '9') {
			width *= 10;
			width += ch - '0';
			ch = *(++fmt);
		}

		/* TODO - Probably print error instead */
		if (!*fmt) return;
		++fmt;
		switch (ch)
		{
			case 'c':
			case 'C':
				{
					uint64_t c = va_arg(ap, uint64_t);
					buffer[0] = c;
					padstr(buffer, 1, width, leftadjust, padchar);
				}
				break;
			case 'd':
			case 'D':
				{
					int64_t idx = convert_decimal(buffer, va_arg(ap, int64_t));
					padstr(&buffer[idx], BUFFER_LEN-idx, width, leftadjust, padchar);
				}
				break;
			case 'u':
			case 'U':
				{
					int64_t idx = convert_decimal_u(buffer, va_arg(ap, uint64_t));
					padstr(&buffer[idx], BUFFER_LEN-idx, width, leftadjust, padchar);
				}
				break;
			case 's':
			case 'S':
				{
					const char* str = va_arg(ap, const char*);
					padstr(str, string_length(str), width, leftadjust, padchar);
				}
				break;
			case 'x':
			case 'X':
				{
					int64_t idx = convert_hexidecimal(buffer, va_arg(ap, uint64_t));
					padstr(&buffer[idx], BUFFER_LEN-idx, width, leftadjust, padchar);
				}
				break;
			case 'o':
			case 'O':
				{
					int64_t idx = convert_octal(buffer, va_arg(ap, uint64_t));
					padstr(&buffer[idx], BUFFER_LEN-idx, width, leftadjust, padchar);
				}
				break;
			case '%':
				{
					write_char('%');
				}
				break;
			default:
				{
					write_char('B');
					panic("kprintf: Bad format string");
				}
				break;
		}
	}
}

void pad(uint64_t extra, char padchar)
{
	for (; extra > 0; --extra)
	{
		write_char(padchar);
	}
}

uint64_t string_length(const char* str)
{
	uint64_t len = 0;
	while (*str)
	{
		++str;
		++len;
	}

	return len;
}

void padstr(const char* str, int64_t len, int64_t width, uint8_t leftadjust, char padchar)
{
	//uint64_t len = string_length(str);	
	int64_t extra = width - len;

	if (extra > 0 && !leftadjust)
	{
		pad(extra, padchar);
	}

	for (int64_t i = 0; i < len; ++i)
	{
		write_char(str[i]);
	}

	if (extra > 0 && leftadjust)
	{
		pad(extra, padchar);
	}
}

const char* hexdigits = "0123456789ABCDEF";

int64_t convert_hexidecimal(char buf[static BUFFER_LEN], uint64_t value)
{
	int64_t index = BUFFER_LEN-1;
	do
	{
		buf[index--] = hexdigits[value % 16];
		value /= 16;
	} while (index >= 0 && value != 0);

	return index+1;
}

int64_t convert_decimal_u(char buf[static BUFFER_LEN], uint64_t value)
{
	int64_t index = BUFFER_LEN-1;

	do	
	{
		buf[index--] = value % 10 + '0';
		value /= 10;
	} while (index >= 0 && value != 0);

	return index+1;
}

int64_t convert_decimal(char buf[static BUFFER_LEN], int64_t value)
{
	int64_t index = BUFFER_LEN-1;
	uint8_t negative = 0;
	if (value < 0)
	{
		negative = 1;
		value = -value;
	}

	do	
	{
		buf[index--] = value % 10 + '0';
		value /= 10;
	} while (index >= 0 && value != 0);

	if (negative)
	{
		if (index >= 0)
		{
			panic("convert_decimal: Bad index");
		}
		buf[index--] = '-';
	}

	return index+1;
}

int64_t convert_octal(char buf[static BUFFER_LEN], uint64_t value)
{
	int64_t index = BUFFER_LEN-1;
	
	do
	{
		buf[index--] = value % 8 + '0';
		value /= 8;
	} while (index >= 0 && value != 0);

	return index+1;
}
