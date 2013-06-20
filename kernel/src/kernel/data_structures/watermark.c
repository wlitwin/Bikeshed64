#include "watermark.h"

#include "kernel/panic.h"
#include "kernel/virt_memory/defs.h"

void water_mark_init(WaterMarkAllocator* wma, const void* location)
{
	wma->current_address = (uint64_t)location;
	wma->page_address = (uint64_t)location;
}

void* water_mark_alloc(WaterMarkAllocator* wma, const uint64_t size)
{
	wma->current_address -= size;
	while (wma->current_address < wma->page_address)
	{
		const uint64_t location = (wma->page_address - PAGE_LARGE_SIZE) + 1;
		if (!virt_map_page(kernel_table, location, PG_FLAG_RW, PAGE_LARGE))
		{
			panic("WaterMark: Failed to allocate page!");
		}

		wma->page_address -= PAGE_LARGE_SIZE;
	}

	void* returnVal = (void*)(wma->current_address+1);

	return returnVal;
}
