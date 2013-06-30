#ifndef __ULIB_H__
#define __ULIB_H__

#include "inttypes.h"
#include "kernel/timer/defs.h"
#include "kernel/syscalls/types.h"

Status fork(Pid* pid);

void msleep(time_t ms);

void exit(void);

#endif
