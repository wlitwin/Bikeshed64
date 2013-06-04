#include "physical.h"
#include "inttypes.h"

#include "arch/x86_64/kprintf.h"
#include "arch/x86_64/virt_memory/types.h"

#define MMAP_MAX_ENTRIES ((0x7C00 - 0x2D04) / 24) /* 24 byte entries for E280 BIOS function */

#define KERNEL_START 0x100000
#define KERNEL_END ((uint64_t)&__KERNEL_END)

#define _1_KIB 1024ULL
#define _1_MIB (1024ULL*_1_KIB)
#define _2_MiB (2ULL*_1_MIB)
#define _1_GIB (1024ULL*_1_MIB)
#define _512_GIB (512ULL*_1_GIB)

#define PDT_PER_PDPT 512ULL

//=============================================================================
// Memory Map BIOS definitions
//=============================================================================

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

/* How many physical address bits the processor has
 *
 * Defined in prekernel.s
 */
extern uint8_t processor_phys_bits;

/* This variable is located at the very end of the kernel
 * so we can place things after it's address in RAM.
 */
extern uint64_t __KERNEL_END;

static uint64_t get_total_usable_ram(void);

/* Initialize the physical memory sub-system
 */
void phys_memory_init()
{
	/* At this point the prekernel.s code has setup a 1GiB identity
	 * mapping of physical ram for us. This means we can place data
	 * structures after the kernel. This avoids the problem of how
	 * to setup physical memory management when paging is already
	 * enabled. There are two things this code will do:
	 *
	 * 1.) Create a virtual mapping for all of physical ram in kernel
	 *     space (Above 0xFFFF...800...).
	 * 2.) Create a physical memory allocator that will allocate physical
	 *     memory in 2MiB and 4KiB chunks.
	 */

	/* Now we can loop through this array and figure out		
	 * how much physical ram is actually free to use by
	 * us.
	 */
	//kprintf("Memory Map Size: %d\n", mmap_size);

	uint64_t total_usable_ram = get_total_usable_ram(); // In Bytes

	/* Let's calculate how much memory we need for the paging structures so we
	 * can do a mapping of all the available (usable) physical memory. We'll
	 * have 2MiB mappings because 1GiB is not supported by all processors. We
	 * could check for it, but to keep things simple we'll choose the one that
	 * should always work. We also ignore the flooring that happens for both
	 * calculations because we already have a statically allocated PDPT and PDT
	 * from perkernel.s.
	 */
	const uint64_t num_pds = total_usable_ram / _2_MiB;

	// PDPTs store 512 pointers to PDs
	const uint64_t num_pdpts = num_pds / PDT_PER_PDPT;

	// Calculate how many bytes we need for all of this information
	const uint64_t space_needed = sizeof(PD_Table)*num_pds + sizeof(PDP_Table)*num_pdpts;

	// Let's try to place this whole table directly after the kernel. This may not always
	// work, but for now it's the simplest approach. We'll display an error if we cannot
	// setup the paging structures directly after the kernel.
	
	const uint32_t mmap_size = *((uint32_t*) MMAP_COUNT);
	MMapEntry* mmap_array = (MMapEntry*) MMAP_ADDRESS;
	
	kprintf("Total Usable RAM: %u MiB\n", total_usable_ram/_1_MIB);
	kprintf("Need %d PDs\n", num_pds);
	kprintf("Need %d PDPTs\n", num_pdpts);
	kprintf("Need %d KiB space\n", space_needed/_1_KIB);
}

uint64_t get_total_usable_ram(void)
{
	/* Before we can continue we need to check what the BIOS has told us
	 * is available for general use.
	 */
	const uint32_t mmap_size = (*(uint32_t*) MMAP_COUNT);
	MMapEntry* mmap_array = (MMapEntry*) MMAP_ADDRESS;

	uint32_t mmap_add_index = mmap_size;
	uint64_t total_usable_ram = 0;

	for (uint32_t i = 0; i < mmap_size; ++i)
	{
		const uint64_t base = mmap_array[i].base;
		const uint64_t length = mmap_array[i].length;
		const uint64_t max_addr = base + length;

		/* Check if this memory region conflicts with the kernel. If it does
		 * we may need to split it into two regions and adjust the mmap_count.
		 *
		 * We have four possibilities:
		 *  1.) The region extends into the start of the kernel
		 *  2.) The region begins inside the kernel and extends past the kernel
		 *  3.) The region is completely inside the kernel
		 *  4.) The region is larger than the kernel and needs to be split
		 *      into two separate regions
		 */			
		/* Case 1 */
		if ((base < KERNEL_START) && max_addr <= KERNEL_END)
		{
			// Update the length
			mmap_array[i].length = KERNEL_START - base;
		}
		/* Case 2 */
		else if (base < KERNEL_END && max_addr > KERNEL_END)
		{
			// Update the base and length
			mmap_array[i].base = KERNEL_END;
			mmap_array[i].length = max_addr - base;
		}
		/* Case 3 */
		else if (base >= KERNEL_START && max_addr <= KERNEL_END)
		{
			// We need to completely remove this region, just mark
			// its type as reserved so we don't use it later
			mmap_array[i].type = TYPE_RESERVED;
		}
		/* Case 4 */
		else if (base < KERNEL_START && max_addr > KERNEL_END)
		{
			// The hardest case, we have to create a new entry
			// in the mmap_array. But we need to make sure we
			// don't add too many entries
			if (mmap_add_index < MMAP_MAX_ENTRIES)
			{
				// Shrink the current entry and add a new entry later	
				mmap_array[i].length = KERNEL_START - base;
				mmap_array[mmap_add_index].base = KERNEL_END;
				mmap_array[mmap_add_index].length = max_addr - KERNEL_END;
				mmap_array[mmap_add_index].type = TYPE_USABLE;
				++mmap_add_index;
			}
			else
			{
				// We can only keep one region, so figure out which one
				// is bigger and we'll keep that region
				if ((KERNEL_START - base) > (max_addr - KERNEL_END))
				{
					mmap_array[i].length = KERNEL_START - base;
				}
				else
				{
					mmap_array[i].base = KERNEL_END;
					mmap_array[i].length = max_addr - KERNEL_END;
				}
			}
		}

		kprintf("%d - Base: 0x%x - Length: 0x%x - Type: %d\n", 
				i, 
				mmap_array[i].base, 
				mmap_array[i],length, 
				mmap_array[i].type);

		if (mmap_array[i].type == TYPE_USABLE)
		{
			total_usable_ram += mmap_array[i].length;
		}
	}

	kprintf("Extras:\n");
	// Loop through any added entries as well
	for (uint32_t i = mmap_size; i < mmap_add_index; ++i)
	{
		total_usable_ram += mmap_array[i].length;
		kprintf("%d - Base: 0x%x - Length: 0x%x - Type: %d\n", 
				i, 
				mmap_array[i].base, 
				mmap_array[i].length, 
				mmap_array[i].type);
	}

	// Update the mmap_size value
	*((uint32_t*) MMAP_COUNT) = mmap_add_index;

	// Return the amount of available ram
	return total_usable_ram;
}
