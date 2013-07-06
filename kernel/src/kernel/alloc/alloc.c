#include "alloc.h"

#include "inttypes.h"

WaterMarkAllocator kernel_WaterMark;

void alloc_init()
{
	// Give it 4MiB of space
	water_mark_init(&kernel_WaterMark, (void*)0xFFFFFFFFFFE00000, 0x400000);
}
