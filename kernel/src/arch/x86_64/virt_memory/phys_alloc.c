#include "phys_alloc.h"

#include "stack.h"
#include "paging.h"
#include "physical.h"

#include "inttypes.h"

#include "arch/x86_64/kprintf.h"

static Stack stack_2MIB;
static Stack stack_4KIB;

static void test_2MIB_alloc(void);

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
	stack_init(&stack_4KIB);

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

		kprintf("Old: 0x%x - 0x%x\n", base_orig, length_orig);
		kprintf("New: 0x%x - 0x%x\n", base, length);

		// Okay we can place something here
		uint64_t count = 0;
		do
		{
			// Check if the stack already contains this node
			//kprintf("%u Base: 0x%x - Left: 0x%x \n", count, base, length);
			/*StackNode* cur1 = stack_2MIB.start;
			for (uint64_t i = 0; i < stack_2MIB.size; ++i)
			{
				StackNode* cur2 = stack_2MIB.start;
				for (uint64_t j = 0; j < stack_2MIB.size; ++j)
				{
					if (cur1 == cur2 && i != j)
					{
						kprintf("PROBLEM: I1: %u - I2: %u  \n", i, j);
						kprintf("Address: 0x%x - 0x%x \n", cur1, cur2);
						__asm__("hlt");
					}
					cur2 = cur2->next;
				}
				cur1 = cur1->next;
			}
			*/

			StackNode* node = (StackNode*) base;
			stack_push(&stack_2MIB, node);
			++count;
			allocatable_ram += _2_MiB;

			base += _2_MiB;
			length -= _2_MiB;
		} while (length >= _2_MiB);

		++usable;
		wasted_ram += length;
	}

	kprintf("Wasted: %u bytes (%u KiB) of RAM\n", wasted_ram, wasted_ram/_1_KIB);
	kprintf("Total allocatable RAM: %u MiB\n", allocatable_ram/_1_MIB);

	test_2MIB_alloc();
}

void test_2MIB_alloc()
{
	kprintf("Stack Size: %u  \n", stack_2MIB.size);

	void* ptr = phys_alloc_2MIB();
	while (ptr != NULL)
	{
		uint64_t* p = (uint64_t*)ptr;
		kprintf("Address: 0x%x - Left: %u   \n", p, stack_2MIB.size);
		*p = 10;
		ptr = phys_alloc_2MIB();
	}

	kprintf("Passed 2MiB Test            \n");
	__asm__("hlt");
}

void* phys_alloc_2MIB()
{
	return stack_pop(&stack_2MIB);
}

void phys_free_2MIB(void* ptr)
{
	stack_push(&stack_2MIB, (void*)MASK_2MIB((uint64_t)ptr));
}

typedef struct
{
	uint32_t num_allocated;
	uint32_t max_available;
	void* implicit_next;
} Pool;

void* phys_alloc_4KIB()
{
	if (!stack_empty(&stack_4KIB))
	{
		return stack_pop(&stack_4KIB);
	}
	else if (!stack_empty(&stack_2MIB))
	{
		// Create a new 4KIB pool
		Pool* pool = (Pool*)stack_pop(&stack_2MIB);

		// The first entry in the pool does book keeping
		pool->num_allocated = 0;
		pool->max_available = (_2_MiB / _4_KIB) - 1;
		pool->implicit_next = (void*)((uint64_t)pool + _4_KIB);
	}

	return NULL;
}

void phys_free_4KIB(void* ptr)
{
	
}
