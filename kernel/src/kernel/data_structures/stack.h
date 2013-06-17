#ifndef __X86_64_VIRT_MEMORY_STACK_H__
#define __X86_64_VIRT_MEMORY_STACK_H__

#include "inttypes.h"

/* Stack node pointer. Points to the next value
 * on the stack.
 *
 * There is no data element because this stack is
 * meant to be used in a way that the stack node
 * itself is the data. It's used as a building
 * block for free list based allocators.
 */
typedef struct _StackNode
{
	struct _StackNode* next;
} StackNode;

/* The stack object. Very simple, stores the current
 * stack size and a pointer to the top element
 */
typedef struct 
{
	uint64_t size;
	StackNode* top;
} Stack;

/* Initialize a stack object. Sets the stack size to 0 and
 * the top pointer to null
 *
 * Parameters:
 *    stack - The stack to initialize
 */
void stack_init(Stack* stack);

/* Push a node onto the stack. Also increments the size of
 * the stack
 *
 * Parameters:
 *    stack - The stack to push an element onto
 *    node - The element to push
 */
void stack_push(Stack* stack, StackNode* node);

/* Pop a value from the stack. If the stack is already empty
 * it returns NULL
 *
 * Parameters:
 *    stack - The stack to pop an element from
 *
 * Returns:
 *    A pointer to the popped StackNode, or NULL if the stack
 *    is already empty
 */
StackNode* stack_pop(Stack* stack);

/* Check if a stack is empty
 *
 * Parameters:
 *    stack - The stack to check
 *
 * Returns:
 *    1 if the stack is empty, 0 otherwise
 */
uint8_t stack_empty(const Stack* stack);

/* Get the size of the stack
 *
 * Parameters:
 *    stack - The stack to find the size of 
 *
 * Returns:
 *    The size of the stack
 */
uint64_t stack_size(const Stack* stack);

#endif

