#ifndef __ULIB_H__
#define __ULIB_H__

#include "inttypes.h"
#include "kernel/syscalls/types.h"

Status fork(Pid* pid);

Status msleep(uint64_t ms);

Status exit(void);

#endif