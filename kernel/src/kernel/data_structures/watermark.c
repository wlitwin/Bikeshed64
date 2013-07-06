#include "watermark.h"

#include "kernel/panic.h"
#include "kernel/virt_memory/defs.h"

void water_mark_init(WaterMarkAllocator* wma, const void* location, const uint64_t max_size)
{
	wma->current_address = (uint64_t)location;
	wma->page_address = (uint64_t)location;
	wma->current_size = 0;
	wma->max_size = max_size;
}

void* water_mark_alloc(WaterMarkAllocator* wma, const uint64_t size)
{
	return water_mark_alloc_align(wma, 1, size);
}

void* water_mark_alloc_align(WaterMarkAllocator* wma, const uint64_t alignment, const uint64_t size)
{
	// Make sure the alignment is a power of two
	ASSERT((alignment != 0) && ((alignment & (alignment - 1)) == 0));

	// Figure out how much padding until the next alignment
	const uint64_t new_address = ALIGN(wma->current_address, alignment);
	const uint64_t adjusted_size = (new_address - wma->current_address) + size;

	if (wma->current_size + adjusted_size > wma->max_size)
	{
		panic("Watermark: Size exceeded!");
	}

	wma->current_address -= adjusted_size;
	wma->current_size += adjusted_size;
	while (wma->current_address < wma->page_address)
	{
		const uint64_t location = (wma->page_address - PAGE_LARGE_SIZE);
		if (!virt_map_page(kernel_table, location, PG_FLAG_RW, PAGE_LARGE, NULL))
		{
			panic("WaterMark: Failed to allocate page!");
		}

		wma->page_address -= PAGE_LARGE_SIZE;
	}

	void* returnVal = (void*)(wma->current_address);

	return returnVal;
}
