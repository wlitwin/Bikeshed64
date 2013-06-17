#ifndef __KERNEL_ALLOC_H__
#define __KERNEL_ALLOC_H__

#include "kernel/data_structures/watermark.h"

/* A simple allocator to make it easier to setup other parts
 * of the kernel. Might be replaced by a more general kmalloc()
 * later.
 */
extern WaterMarkAllocator kernel_WaterMark;

/* Initialize all kernel memory allocators.
 */
void alloc_init(void);

#endif
