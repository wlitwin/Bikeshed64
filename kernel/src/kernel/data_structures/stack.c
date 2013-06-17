#include "stack.h"


uint64_t stack_size(const Stack* stack)
{
	return stack->size;
}

void stack_init(Stack* stack)
{
	stack->size = 0;
	stack->top = NULL;
}

void stack_push(Stack* stack, StackNode* node)
{
	if (stack->top == NULL)
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

uint8_t stack_empty(const Stack* stack)
{
	return stack->size == 0;
}

StackNode* stack_pop(Stack* stack)
{
	if (stack->top == NULL)
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
