#include "alloc.h"

#include "inttypes.h"

WaterMarkAllocator kernel_WaterMark;

void alloc_init()
{
	water_mark_init(&kernel_WaterMark, (void*)0xFFFFFFFFFFFFFFFF);
}
