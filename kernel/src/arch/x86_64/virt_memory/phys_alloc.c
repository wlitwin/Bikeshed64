#include "phys_alloc.h"

#include "paging.h"
#include "physical.h"

#include "inttypes.h"

#include "kernel/data_structures/stack.h"

#include "arch/x86_64/panic.h"
#include "arch/x86_64/kprintf.h"

static Stack stack_2MIB;

typedef struct _Pool
{
	Stack free_stack;
	void* implicit_next;
	void* max_address;
	
	struct _Pool* next;
	struct _Pool* prev;

	uint8_t on_list;
} Pool;

static Pool* pool_4KIB;

//static void test_2MIB_alloc(void);
//static void test_4KIB_alloc(void);

/* 
 */
void setup_physical_allocator()
{
	// We have two separate lists, one for 4KiB segments and one for 2MiB 
	// segments. If more 4KiB segments are needed, then a 2MiB segment is
	// split into smaller 4KiB segments.
	
	const uint32_t mmap_size = *((uint32_t*) MMAP_COUNT);
	MMapEntry* mmap_array = (MMapEntry*) MMAP_ADDRESS;

	// All of physical memory is mapped starting at the kernels half of the
	// address space. Therefore in order to get a physical address the kernel's
	// base address needs to be added to it.
	
	stack_init(&stack_2MIB);
	pool_4KIB = NULL;

	uint64_t wasted_ram = 0;
	uint64_t allocatable_ram = 0;

	uint32_t usable = 0;
	for (uint32_t i = 0; i < mmap_size; ++i)
	{
		if (mmap_array[i].type != TYPE_USABLE)
		{
			continue;
		}

		const uint64_t base_orig = mmap_array[i].base;	
		const uint64_t length_orig = mmap_array[i].length;

		if (length_orig < _2_MiB)
		{
			wasted_ram += length_orig;
			continue;
		}

		uint64_t base = ALIGN_2MIB(base_orig);
		uint64_t length = length_orig - (base - base_orig);

		if (length < _2_MiB)
		{
			wasted_ram += length_orig;
			continue;
		}

		// Okay we can place something here
		uint64_t count = 0;
		do
		{
			StackNode* node = (StackNode*) (base + KERNEL_BASE);
			stack_push(&stack_2MIB, node);
			++count;
			allocatable_ram += _2_MiB;

			base += _2_MiB;
			length -= _2_MiB;
		} while (length >= _2_MiB);

		++usable;
		wasted_ram += length;
	}

	//test_2MIB_alloc();
	//test_4KIB_alloc();
}

/*
void test_2MIB_alloc()
{
	kprintf("Stack Size: %u  \n", stack_size(&stack_2MIB));

	void* ptr = phys_alloc_2MIB();
	kprintf("Address: 0x%x - Left: %u   \n", ptr, stack_size(&stack_2MIB));
	while (ptr != NULL)
	{
		uint64_t* p = (uint64_t*)ptr;
		kprintf("Address: 0x%x - Left: %u   \n", p, stack_size(&stack_2MIB));
		*p = 10;
		ptr = phys_alloc_2MIB();
	}

	kprintf("Next: 0x%x \n", ptr);

	kprintf("Passed 2MiB Test            \n");
	__asm__("hlt");
}
*/

/*
void test_4KIB_alloc()
{
	void* ptr = phys_alloc_4KIB();
	kprintf("Start: 0x%x  \n", ptr);
	while (ptr != NULL)
	{
		uint64_t* p = (uint64_t*)ptr;
		//kprintf("Address: 0x%x\n", p);
		*p = 10;
		ptr = phys_alloc_4KIB();
	}

	kprintf("Passed 4KiB Test            \n");
//	__asm__("hlt");
}
*/

void* phys_alloc_2MIB()
{
	void* retVal = stack_pop(&stack_2MIB);
	if (retVal == 0)
	{
		return NULL;
	}

	void* final_value = VIRT_TO_PHYS(retVal);

#ifdef DEBUG_PHYS_ALLOC
	kprintf("2MIB: 0x%x \n", final_value);
#endif

	return final_value;
}

void phys_free_2MIB(void* ptr)
{
	const uint64_t address = (uint64_t)PHYS_TO_VIRT(ptr);
	stack_push(&stack_2MIB, (void*)MASK_2MIB(address));
}

static void pool_init(Pool* pool)
{
	stack_init(&pool->free_stack);	
	pool->implicit_next = (void*) ((uint64_t)pool + _4_KIB);
	pool->max_address = (void*) ((uint64_t)pool + _2_MiB);
	pool->next = NULL;
	pool->prev = NULL;
	pool->on_list = 0;

#ifdef DEBUG_PHYS_ALLOC
	kprintf("POOL INIT\n");
	kprintf("implicit_next: 0x%x\n", pool->implicit_next);
	kprintf("max_address:   0x%x\n", pool->max_address);
	kprintf("next:          0x%x\n", pool->next);
	kprintf("prev:          0x%x\n", pool->prev);
	kprintf("on_list:       0x%x\n", pool->on_list);
#endif
}

static uint8_t pool_empty(Pool* pool)
{
	return stack_empty(&pool->free_stack) 
		&& pool->implicit_next >= pool->max_address;
}

static uint8_t pool_full(Pool* pool)
{
	if (pool->implicit_next < pool->max_address)
	{
		panic("Something wrong with pool!");
	}

	return stack_size(&pool->free_stack) == 511;
}

static void pool_free(Pool* pool, void* ptr)
{
	if (ptr <= (void*)pool || ptr >= pool->max_address)
	{
		kprintf("POOL: 0x%x - PTR: 0x%x - MAX: 0x%x \n", pool, ptr, pool->max_address);
		panic("Pool freeing bad ptr");
	}

	stack_push(&pool->free_stack, (void*) MASK_4KIB(ptr));
}

static void* pool_alloc(Pool* pool)
{
	if (pool_empty(pool))
	{
		panic("Trying to allocate from empty pool!");
	}

	if (!stack_empty(&pool->free_stack))
	{
		return stack_pop(&pool->free_stack);
	}
	else
	{
		void* retVal = pool->implicit_next;
		pool->implicit_next = (void*) ((uint64_t)pool->implicit_next + _4_KIB);

		return retVal;
	}
}

void* phys_alloc_4KIB()
{
	if (pool_4KIB == NULL)
	{
		pool_4KIB = (Pool*) phys_alloc_2MIB();
		if (pool_4KIB == NULL)
		{
			return NULL;
		}

		pool_4KIB = (Pool*) PHYS_TO_VIRT(pool_4KIB);

		pool_init(pool_4KIB);
		pool_4KIB->on_list = 1;
	}

	uint64_t retVal = (uint64_t) pool_alloc(pool_4KIB);	
	if (pool_empty(pool_4KIB))
	{
		Pool* p_next = pool_4KIB->next;
		pool_4KIB->on_list = 0;
		pool_4KIB->next = NULL;
		pool_4KIB->prev = NULL;

		pool_4KIB = p_next;
	}

	void* final_value = VIRT_TO_PHYS(retVal);
#ifdef DEBUG_PHYS_ALLOC
	kprintf("4KIB: 0x%x \n", final_value);
#endif
	return final_value;
}

void phys_free_4KIB(void* ptr)
{
	// Figure out which pool it belongs to	
	const uint64_t address = (uint64_t)PHYS_TO_VIRT(ptr);
	Pool* pool = (Pool*) MASK_2MIB(address);

	pool_free(pool, (void*)address);

	if (pool_4KIB == NULL)
	{
		panic("phys_free_4KIB bad free");
	}

	// Check if this pool is already in the pool list	
	if (pool->on_list && pool_full(pool))
	{
		// We need to remove it from the list and free the 2MIB
		// chunk of memory it's using

		if (pool_4KIB == pool)
		{
			// It's the head of the list
			pool_4KIB = pool_4KIB->next;
			if (pool_4KIB != NULL)
			{
				pool_4KIB->prev = NULL;
			}
		}
		else
		{
			// It's in the middle of the list
			Pool* p_next = pool->next;	
			Pool* p_prev = pool->prev;

			if (p_next != NULL)
			{
				p_next->prev = p_prev;
			}

			if (p_prev != NULL)
			{
				p_prev->next = p_next;
			}
		}

		// Reset the pool just in case
		pool_init(pool);
		phys_free_2MIB(pool);
	}
	else if (!pool->on_list)
	{
		// Add to the head of the list
		pool_4KIB->prev = pool;
		pool->next = pool_4KIB;
		pool->prev = NULL;

		pool_4KIB = pool;
		pool_4KIB->on_list = 1;
	}
}
