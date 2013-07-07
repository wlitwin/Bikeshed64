#ifndef __ULIB_H__
#define __ULIB_H__

#include "inttypes.h"
#include "kernel/timer/defs.h"
#include "kernel/syscalls/types.h"

Status fork(Pid* pid);

void msleep(time_t ms);

void exit(void);

void set_priority(uint8_t priority);

uint8_t key_available(void);

uint8_t get_key(void);

// Blocks
uint8_t read_key(void);

#endif
