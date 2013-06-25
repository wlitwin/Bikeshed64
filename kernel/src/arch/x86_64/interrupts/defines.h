#ifndef __X86_64_INTERRUPTS_DEFINES_H__
#define __X86_64_INTERRUPTS_DEFINES_H__

#define GDT_CODE_SEG 0x10
#define GDT_DATA_SEG 0x20

#define SP rsp
#define IP rip
#define BP rbp
#define FLAGS rflags
#define DEFAULT_EFLAGS 0x202 // IF + reserved bit

#endif
