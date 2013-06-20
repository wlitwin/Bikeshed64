#include "stack.h"

void stack_init(Stack* stack)
{
	stack->size = 0;
	stack->top = NULL;
}

void stack_push(Stack* stack, StackNode* node)
{
	if (stack_empty(stack))
	{
		node->next = NULL;
		stack->size = 1;
	}
	else
	{
		node->next = stack->top;
		++stack->size;
	}

	stack->top = node;
}

StackNode* stack_pop(Stack* stack)
{
	if (stack_empty(stack))
	{
		return NULL;
	}
	else
	{
		StackNode* returnVal = stack->top;
		stack->top = returnVal->next;
		--stack->size;

		return returnVal;
	}
}
