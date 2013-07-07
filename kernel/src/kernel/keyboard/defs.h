#ifndef __KERNEL_KEYBOARD_H__
#define __KERNEL_KEYBOARD_H__

void keyboard_init(void);

uint8_t keyboard_char_available(void);

uint8_t keyboard_get_char(void);

#endif
