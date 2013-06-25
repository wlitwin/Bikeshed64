#ifndef __KERNEL_SYSCALLS_TYPES_H__
#define __KERNEL_SYSCALLS_TYPES_H__

#include "kernel/scheduler/pcb.h"

typedef enum
{
	SUCCESS = 0, 
	FAILURE,
	BAD_PARAM,
	FEATURE_UNIMPLEMENTED,
} Status;

#endif
