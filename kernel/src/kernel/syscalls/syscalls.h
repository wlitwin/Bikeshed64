#ifndef __KERNEL_SYSCALLS_H__
#define __KERNEL_SYSCALLS_H__

#define NUM_SYSCALLS      7
#define SYSCALL_FORK      0
#define SYSCALL_EXEC      1
#define SYSCALL_EXIT      2
#define SYSCALL_MSLEEP    3
#define SYSCALL_SET_PRIO  4
#define SYSCALL_KEY_AVAIL 5
#define SYSCALL_GET_KEY   6

#ifdef BIKESHED_X86_64
#define SYSCALL_INT_VEC 0x80
#else
#error "Syscalls not implemented for this architecture"
#endif

void syscalls_init(void);

#endif
