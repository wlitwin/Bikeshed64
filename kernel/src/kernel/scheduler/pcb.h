#ifndef __SCHEDULER_PCB_H__
#define __SCHEDULER_PCB_H__

#include "inttypes.h"

#ifdef BIKESHED_X86_64
#include "arch/x86_64/interrupts/imports.h"
#endif

typedef uint8_t Priority;
typedef uint32_t Pid;

typedef enum
{
	READY = 0,
	RUNNING,
	KILLED,
} State;

typedef struct
{
	// 8 byte fields
	Context* context;
	void* page_table;

	// 2 byte fields
	Pid pid;
	Pid ppid;

	// 1 byte fields
	State state;
	Priority priority;
} PCB;

typedef struct _Thread
{
	// 8 byte fields
	PCB* pcb;
	Context* context;
	struct _Thread* next;
	struct _Thread* prev;

	// 1 byte fields
	State state;
	Priority priority;
} Thread;

#endif
