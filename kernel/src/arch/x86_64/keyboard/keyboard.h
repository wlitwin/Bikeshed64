#ifndef __X86_64_KEYBOARD_H__
#define __X86_64_KEYBOARD_H__

#include "inttypes.h"

void keyboard_init(void);

uint8_t keyboard_char_available(void);

uint8_t keyboard_get_char(void);

#endif
