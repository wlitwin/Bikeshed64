#ifndef __X86_64_VIRT_MEMORY_STACK_H__
#define __X86_64_VIRT_MEMORY_STACK_H__

#include "inttypes.h"

#define NULL ((void*)0)

typedef struct _StackNode
{
	struct _StackNode* next;
} StackNode;

typedef struct
{
	uint64_t size;
	StackNode* start;
} Stack;

void stack_init(Stack* stack);

void stack_push(Stack* stack, StackNode* node);

uint8_t stack_empty(Stack* stack);

StackNode* stack_pop(Stack* stack);

#endif
