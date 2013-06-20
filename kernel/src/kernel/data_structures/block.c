#include "block.h"

#include "safety.h"
#include "kernel/panic.h"
#include "kernel/kprintf.h"

BlockAllocator* block_init(const void* address, 
						const uint64_t length, 
						const uint64_t block_size)
{
	if (length < sizeof(BlockAllocator) || 
		block_size < sizeof(StackNode))
	{
		panic("BlockAllocator - Init: Region of memory too small");	
	}

	BlockAllocator* ba = (BlockAllocator*) address;
	ba->base = (uint64_t)address + sizeof(BlockAllocator);
	const uint64_t adjusted_size = length - sizeof(BlockAllocator);
	const uint64_t max_blocks = adjusted_size / block_size;
	ba->block_size = block_size;
	ba->max_address = ba->base + max_blocks * block_size;
	ba->implicit_next = ba->base;
	stack_init(&ba->free_stack);

#ifdef DEBUG_BLOCK_ALLOCATOR
	kprintf("BA: 0x%x\n", address);
	kprintf("MB: %u\n", max_blocks);
	kprintf("block_size: %u\n", block_size);
	kprintf("max_addr: 0x%x\n", ba->max_address);
	kprintf("implicit: 0x%x\n", ba->implicit_next);
#endif

	return ba;
}

void* block_alloc(BlockAllocator* ba)
{
	if (!stack_empty(&ba->free_stack))
	{
		return stack_pop(&ba->free_stack);
	}
	else if (ba->implicit_next < ba->max_address)
	{
		void* returnVal = (void*)ba->implicit_next;
		ba->implicit_next += ba->block_size;

		return returnVal;
	}

	return NULL;
}

void block_free(BlockAllocator* ba, void* ptr)
{
	ASSERT(((uint64_t)ptr) >= ((uint64_t)ba+sizeof(BlockAllocator)) &&
			(uint64_t)ptr < ba->max_address);
	stack_push(&ba->free_stack, ptr);		
}
