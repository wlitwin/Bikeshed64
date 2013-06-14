#ifndef __VIRT_MEMORY_PHYSICAL_H__
#define __VIRT_MEMORY_PHYSICAL_H__

#include "inttypes.h"

/* This variable is located at the very end of the kernel
 * so we can place things after it's address in RAM.
 */
extern uint64_t __KERNEL_END;

#define KERNEL_START 0x100000
#define KERNEL_END ((uint64_t)&__KERNEL_END)

#define KERNEL_BASE 0xFFFF800000000000

#define PHYS_TO_VPHYS(X) ((void*)((uint64_t)(X) + KERNEL_BASE))

//=============================================================================
// Memory Map BIOS definitions
//=============================================================================

#define MMAP_MAX_ENTRIES ((0x7C00 - 0x2D04) / 24) /* 24 byte entries for E280 BIOS function */

/* Memory Map Entries are given by the BIOS when we use the 0xE820
 * instruction. We need to convert this to something more useful 
 * and give it to the physical memory allocator.
 */

/* Address of the start of the memory map array 
 *
 * Defined in bootloader.s
 */
#define MMAP_ADDRESS 0x2D04

/* Address of the array size 
 *
 * Defined in bootloader.s
 */
#define MMAP_COUNT 0x2D00

#define TYPE_USABLE 1
#define TYPE_RESERVED 2
#define TYPE_ACPI_RECLAIMABLE 3
#define TYPE_ACPI_NVS 4
#define TYPE_BAD_MEMORY 5

typedef struct
{
	uint64_t base;
	uint64_t length;
	uint32_t type; // 1 - Usable RAM
				   // 2 - Reserved
				   // 3 - ACPI Reclaimable
				   // 4 - ACPI NVS 
				   // 5 - Bad Memory
	uint32_t ACPI;
} MMapEntry;

COMPILE_ASSERT(sizeof(MMapEntry) == 24);

//=============================================================================

void phys_memory_init(void);

#endif
