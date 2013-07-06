#ifndef __X86_64_TEXT_MODE_H__
#define __X86_64_TEXT_MODE_H__

void init_text_mode(void);

void text_mode_char(char c);

void page_up(void);

void page_down(void);

void clear_screen(void);

void text_mode_reset(void);

#endif
