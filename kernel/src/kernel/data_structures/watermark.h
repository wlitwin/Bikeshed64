#ifndef __KERNEL_WATER_MARK_H__
#define __KERNEL_WATER_MARK_H__

#include "inttypes.h"

/* Water mark allocator. Only grows in one direction and 
 * never shrinks. Useful during initialization to create
 * regions of memory suitable for other allocators or 
 * data that is variable on startup, but constant later.
 *
 * This object will also handle all the virtual memory calls
 * needed to make sure the regions of memory it gives out
 * are usable by someone else.
 *
 * This allocator grows down and grows in PAGE_LARGE_SIZE 
 * amounts each time it runs out of space.
 */
typedef struct
{
	uint64_t page_address;
	uint64_t current_address;	
	uint64_t current_size;
	uint64_t max_size;
} WaterMarkAllocator;

/* Initialize the watermark object. Doesn't do much.
 *
 * NOTE: This allocator grows down.
 *
 * Parameters:
 *    wma - The WaterMarkAllocator to initialize
 *    location - The maximum address of the allocator
 */
void water_mark_init(WaterMarkAllocator* wma, const void* location, const uint64_t max_size);

/* Allocates a region of memory.
 *
 * Parameters:
 *    wma - The WaterMarkAllocator to allocate from
 *    size - How many bytes to allocate
 *
 * Returns:
 *    A Pointer to the allocated memory, or NULL if 
 *    something went wrong
 */
void* water_mark_alloc(WaterMarkAllocator* wma, const uint64_t size);

/* Allocates a region of memory aligned to a boundary.
 *
 * Parameters:
 *    wma - The WaterMarkAllocator to allocate from
 *    alignment - The byte boundary to align the memory to
 *    size - How many bytes to allocate
 *
 * Returns:
 *    A Pointer to the allocated memory, or NULL if 
 *    something went wrong
 */
void* water_mark_alloc_align(WaterMarkAllocator* wma, const uint64_t alignment, const uint64_t size);

#endif
