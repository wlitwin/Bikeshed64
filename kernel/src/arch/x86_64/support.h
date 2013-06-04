#ifndef __X86_64_SUPPORT_H__
#define __X86_64_SUPPORT_H__

#include "inttypes.h"

extern uint8_t  _inb(uint16_t port);
extern uint16_t _inw(uint16_t port);
extern uint32_t _inl(uint16_t port);

extern void _outb(uint16_t port, uint8_t  val);
extern void _outw(uint16_t port, uint16_t val);
extern void _outl(uint16_t port, uint32_t val);

#endif
