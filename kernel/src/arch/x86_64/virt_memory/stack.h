#ifndef __X86_64_VIRT_MEMORY_STACK_H__
#define __X86_64_VIRT_MEMORY_STACK_H__

#include "inttypes.h"

typedef struct _StackNode
{
	struct _StackNode* next;
} StackNode;

typedef struct
{
	uint64_t size;
	StackNode* start;
} Stack;

#endif
