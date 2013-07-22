#ifndef __X86_64_KPRINTF_H__
#define __X86_64_KPRINTF_H__

/* Standard printf() function. The first argument 
 * is a string, the rest of the arguments are the
 * things to format. Does some error checking, but
 * may not be super robust.
 *
 * Format syntax:
 *
 *   %[-][0][width](c|d|u|s|x|o|%)
 *
 * -     => Left adjust
 * 0     => Pad with zeros
 * width => Amount of padding
 * c     => Character
 * d     => Decimal number
 * u     => Unsigned decimal number
 * b     => Binary number
 * s     => String
 * x     => Hexidecimal number
 * o     => Octal number
 * %     => Print the percent character
 */
void kprintf(const char* format, ...);

#endif
