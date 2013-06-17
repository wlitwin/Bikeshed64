#ifndef __DATA_STUCTURES_BLOCK_H__
#define __DATA_STUCTURES_BLOCK_H__

#include "inttypes.h"

#include "stack.h"

/* Basic block allocator. It can allocate and free blocks of
 * constant size data. All of the allocate and free functions
 * are constant time.
 *
 * The minimum block size is the size of a StackNode and the
 * allocation area must be at least the size of a BlockAllocator
 * object (more to be useful).
 */
typedef struct
{
	uint64_t base;
	uint64_t max_address;
	uint64_t implicit_next;
	uint64_t block_size;
	Stack free_stack;
} BlockAllocator;

/* Initialize a piece of memory to be a BlockAllocator object.
 *
 * The length must be >= sizeof(BlockAllocator)
 * The block_size must be >= to sizeof(StackNode)
 *
 * Parameters:
 *    address - The memory address to create the block allocator at
 *    length - The length of the memory region
 *    block_size - The size of blocks to allocate
 *
 * Returns:
 *    A ready to use BlockAllocator object
 */
BlockAllocator* block_init(const void* address, 
						const uint64_t length, 
						const uint64_t block_size);

/* Allocate a block from the given BlockAllocator object. Returns
 * NULL if no more blocks can be allocated.
 *
 * Parameters:
 *    ba - The BlockAllocator to allocate from
 *
 * Returns:
 *    A pointer to the block, or NULL if no more blocks are available
 */
void* block_alloc(BlockAllocator* ba);

/* Free a block. Does no error checking so avoid doing double frees or
 * frees from invalid memory regions.
 *
 * Parameters:
 *    ba - The block allocator to free to
 *    ptr - A pointer to the block to free
 */
void block_free(BlockAllocator* ba, void* ptr);

#endif
