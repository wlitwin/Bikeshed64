#ifndef __X86_64_INTERRUPTS_DEFINES_H__
#define __X86_64_INTERRUPTS_DEFINES_H__

#define CODE_SEG_64 0x10
#define DATA_SEG_64 0x20
#define USER_CODE_SEG_64 0x30
#define USER_DATA_SEG_64 0x40
#define TSS_SEG_64  0x50

#define SP rsp
#define IP rip
#define BP rbp
#define FLAGS rflags
#define DEFAULT_EFLAGS 0x202 // IF + reserved bit

#endif
