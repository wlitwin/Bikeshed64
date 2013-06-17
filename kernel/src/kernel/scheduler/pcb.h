#ifndef __SCHEDULER_PCB_H__
#define __SCHEDULER_PCB_H__

#include "inttypes.h"

typedef uint8_t State;
typedef uint8_t Priority;
typedef uint32_t Pid;

typedef struct
{
	// 2 byte fields
	Pid pid;
	Pid ppid;

	// 1 byte fields
	State state;
	Priority priority;
	uint8_t quantum;
} PCB;

#endif
