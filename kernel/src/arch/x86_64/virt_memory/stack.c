#include "stack.h"

void stack_init(Stack* stack)
{
	stack->size = 0;
	stack->start = NULL;
}

void stack_push(Stack* stack, StackNode* node)
{
	if (stack->start == NULL)
	{
		node->next = NULL;
		stack->size = 1;
	}
	else
	{
		node->next = stack->start;
		++stack->size;
	}

	stack->start = node;
}

uint8_t stack_empty(Stack* stack)
{
	return stack->size == 0;
}

StackNode* stack_pop(Stack* stack)
{
	if (stack->start == NULL)
	{
		return NULL;
	}
	else
	{
		StackNode* returnVal = stack->start;
		stack->start = returnVal->next;
		--stack->size;

		return returnVal;
	}
}
