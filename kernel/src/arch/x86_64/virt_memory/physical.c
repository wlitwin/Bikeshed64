#include "physical.h"

#include "paging.h"
#include "phys_alloc.h"

#include "kernel/klib.h"

#include "arch/x86_64/panic.h"
#include "arch/x86_64/kprintf.h"
#include "arch/x86_64/virt_memory/types.h"

/* How many physical address bits the processor has
 *
 * Defined in prekernel.s
 */
extern uint8_t processor_phys_bits;
extern uint8_t processor_virt_bits;

typedef struct
{
	uint64_t total_usable_ram;
	uint64_t highest_address;
} ram_info;

static void get_total_usable_ram(ram_info* info);

typedef struct
{
	uint64_t base;         // Memory region base
	uint64_t length;       // Memory region length
	uint64_t num_pds;      // Number of page directories to make
	uint64_t num_pdpts;    // Number of PDP tables to make 
	uint64_t space_needed; // How much memory this will take
} page_struct_info;

static void create_paging_structures(const page_struct_info* psi);

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

	kprintf("Virt Bits: %d - Phys Bits: %d\n", processor_virt_bits, processor_phys_bits);

	ram_info info;
	get_total_usable_ram(&info);

	kprintf("Modified Memory Map\n");
	const uint32_t mmap_size1 = *((uint32_t*) MMAP_COUNT);
	MMapEntry* mmap_array1 = (MMapEntry*) MMAP_ADDRESS;
	for (uint32_t i = 0; i < mmap_size1; ++i)
	{
		kprintf("%d - Base: 0x%x - Length: 0x%x - Type: %d\n", 
				i, 
				mmap_array1[i].base, 
				mmap_array1[i].length, 
				mmap_array1[i].type);
	}
	

	uint64_t total_usable_ram = info.total_usable_ram; // In Bytes

	kprintf("highest address: 0x%x\n", info.highest_address);

	/* Let's calculate how much memory we need for the paging structures so we
	 * can do a mapping of all the available (usable) physical memory. We'll
	 * have 2MiB mappings because 1GiB is not supported by all processors. We
	 * could check for it, but to keep things simple we'll choose the one that
	 * should always work. We also ignore the flooring that happens for both
	 * calculations because we already have a statically allocated PDPT and PDT
	 * from perkernel.s.
	 */
	// TODO - Figure out if we should unmap as well. The computer may have < 1GiB
	//        of RAM installed. (although we wouldn't be able to unallocate anything
	//        of value, we still need the 4KiB for the tables)
	// The +_512_MIB is to fix the flooring that the integer division does
	uint64_t num_pds = (info.highest_address + _512_MiB) / _1_GIB;
	if (num_pds >= 1)
	{
		--num_pds;
	}

	kprintf("Total usable RAM: 0x%x\n", total_usable_ram);

	// PDPTs store 512 pointers to PDs
	// The +1 is to correct the flooring done by integer division
	uint64_t num_pdpts = (num_pds + 1) / PDT_PER_PDPT;
	if (num_pdpts >= 1)
	{
		--num_pdpts;
	}

	// Calculate how many bytes we need for all of this information
	const uint64_t space_needed = sizeof(PD_Table)*num_pds + sizeof(PDP_Table)*num_pdpts;

	kprintf("Space for paging structures: %u KiB %u PD %u PDP\n", 
			space_needed/_1_KIB, num_pds, num_pdpts);

	// Let's try to place this whole table directly after the kernel. This may not always
	// work, but for now it's the simplest approach. We'll display an error if we cannot
	// setup the paging structures directly after the kernel.
	
	const uint32_t mmap_size = *((uint32_t*) MMAP_COUNT);
	MMapEntry* mmap_array = (MMapEntry*) MMAP_ADDRESS;

	// Now we can loop through the memory map array and look for good places to allocate
	// these paging structures. Ideally we want to allocate them after the kernel as the
	// very low memory (< 1 MiB) will be used later for other things.
	//
	// Right now this is a very unsophisticated allocation, we are only looking for a
	// contiguous region of RAM, when we could easily allocate smaller 4KiB parts.
	
	uint8_t found_suitable_region = 0;
	for (uint32_t i = 0; i < mmap_size; ++i)
	{
		if (mmap_array[i].type != TYPE_USABLE)
		{
			continue;
		}

		const uint64_t base_before = mmap_array[i].base;
		const uint64_t length_before = mmap_array[i].length;

		// We're using (and aligning to) 4KiB sizes, so we must have
		// at least that much space available
		if (length_before < _4_KIB)
		{
			continue;
		}

		// Makes the create_paging_structures() code easier if we align
		// the current base to a 4KiB boundary (which is required for
		// all paging structures)
		const uint64_t base = ALIGN_4KIB(base_before);
		const uint64_t length = length_before - (base - base_before);

		if (base >= _1_GIB)
		{
			// We have only mapped the first 1GiB of RAM, so
			// if we are already beyond that we have a problem.
			break;
		}

		if (base >= KERNEL_END && length >= space_needed)
		{

			//kprintf("Using Region: 0x%x - Length: 0x%x\n", base, length);
			page_struct_info psi;

			psi.base = base;
			psi.length = length;
			psi.num_pds = num_pds;
			psi.num_pdpts = num_pdpts;
			psi.space_needed = space_needed;

			// Adjust this entry's base and size
			mmap_array[i].base = base + space_needed;
			mmap_array[i].length = length - space_needed;

			//kprintf("Updated size: 0x%x - Length: 0x%x\n", mmap_array[i].base, mmap_array[i].length);

			create_paging_structures(&psi);

			// We've found our region
			found_suitable_region = 1;
			break;
		}
	}

	if (!found_suitable_region)
	{
		panic("Could not allocate paging structures\n");
	}

	setup_physical_allocator();
	
	kprintf("Total Usable RAM: %u MiB\n", total_usable_ram/_1_MIB);
	kprintf("Need %u PDs\n", num_pds);
	kprintf("Need %u PDPTs\n", num_pdpts);
	kprintf("Need %u KiB space\n", space_needed/_1_KIB);

	// Now clear the lower half entries from the kernel's page table
	PML4_Table* pml4_table = (PML4_Table*) PHYS_TO_VIRT(kernel_table);
	for (uint64_t i = 0; i < 256; ++i)
	{
		pml4_table->entries[i] = 0;	
	}
}

/* This function creates a virtual mapping to all of physical memory inside of
 * kernel space. This is useful because paging doesn't have to be disabled in
 * order to access any byte of physical memory. This will simplify many things
 * later on in the kernel's code.
 */
void create_paging_structures(const page_struct_info* psi)
{
	// If we get here we can allocate the all the memory we need at once
	// and do the mapping at the same time. To be safe we'll do an allocation
	// followed by a mapping in case we use more than 1GiB of RAM for the
	// total mapping. That way those above 1GiB regions should be mapped by
	// the time we get to them.
	
	// We'll fill in the first PDP Table specially because it's statically	
	// allocated. We'll then do a loop for the rest of them.

	uint64_t pds_left = psi->num_pds;
	uint64_t pdpts_left = psi->num_pdpts;

	// Align to a 2MiB boundary
	uint64_t alloc_ptr = psi->base; // Should already be aligned to a 4KiB boundary
	uint64_t phys_address = _1_GIB;

	PDP_Table* pdp_table = &kernel_PDPTE;

	// 0 has already been mapped from 0-1GiB
	for (uint32_t pdpt_index = 1; pdpt_index < PDT_PER_PDPT; ++pdpt_index)
	{
		if (pds_left == 0)
		{
			break;
		}

		PD_Table* pd_table = (PD_Table*) alloc_ptr;	
		alloc_ptr += sizeof(PD_Table);
		--pds_left;

		// Clear this entry
		memclr(pd_table, sizeof(PD_Table));

		pdp_table->entries[pdpt_index] = (uint64_t)pd_table | PDPT_WRITABLE | PDPT_PRESENT;
		invlpg(pd_table);

		for (uint32_t j = 0; j < PDT_ENTRIES; ++j)
		{
			pd_table->entries[j] = phys_address | PDT_PAGE_SIZE | PDT_WRITABLE | PDT_PRESENT;
			invlpg(phys_address);
			phys_address += _2_MiB;

			// TODO invlpg
		}
		invlpg(pd_table);
	}

	// Defined in paging.h
	PML4_Table* pml4_table = &kernel_PML4;

	// TODO rest of the PDPTs
	for (uint64_t pml4_index = 1; pml4_index < PML4_ENTRIES && pdpts_left > 0; ++pml4_index)
	{
		kprintf("Entered PML4 LOOP!\n");
		if (pdpts_left == 0 || pds_left == 0)
		{
			break;
		}

		// Allocate a PDP Table
		pdp_table = (PDP_Table*) alloc_ptr;
		alloc_ptr += sizeof(PDP_Table);
		--pdpts_left;

		// Clear the PDP Table
		memclr(pdp_table, sizeof(PDP_Table));

		pml4_table->entries[pml4_index] = (uint64_t)pdp_table | PML4_WRITABLE | PML4_PRESENT;	
		// TODO replicate this mapping at the kernel's address space
		// pml4_table->entries[pml4_index+256] = ...
		invlpg(pdp_table);
		// TODO invlpg

		// Create this many Page Directory Tables
		for (uint32_t i = 0; i < PDPT_ENTRIES; ++i)
		{
			if (pds_left == 0)
			{
				break;
			}

			PD_Table* pd_table = (PD_Table*) alloc_ptr;
			alloc_ptr += sizeof(PD_Table);
			--pds_left;

			// Clear the PD Table
			memclr(pd_table, sizeof(PD_Table));

			pdp_table->entries[i] = (uint64_t)pd_table | PDPT_WRITABLE | PDPT_PRESENT;
			
			// TODO invlpg
			invlpg(pd_table);

			// Loop through the PD Tables needed
			for (uint32_t j = 0; j < PDT_ENTRIES; ++j)
			{
				pd_table->entries[j] = phys_address | PDT_PAGE_SIZE | PDT_WRITABLE | PDT_PRESENT;
				invlpg(phys_address);
				phys_address += _2_MiB;

				// TODO invlpg
			}
		}
	}
}

/* This function will determine the amount of physical RAM available for general
 * use by the kernel. It is smart enough to know where the kernel is in physical
 * RAM and will update the memory map to avoid having regions that overlap the
 * kernel's current location.
 * 
 * This may involve adding new entries to the memory map array and updating the
 * existing entries. Therefore any old values from the MMAP_ARRAY or MMAP_COUNT
 * locations should not be used, new values should be fetched.
 */
void get_total_usable_ram(ram_info* info)
{
	/* Before we can continue we need to check what the BIOS has told us
	 * is available for general use.
	 */
	const uint32_t mmap_size = (*(uint32_t*) MMAP_COUNT);
	MMapEntry* mmap_array = (MMapEntry*) MMAP_ADDRESS;

	uint32_t mmap_add_index = mmap_size;
	uint64_t highest_address = 0;
	uint64_t total_usable_ram = 0;

	for (uint32_t i = 0; i < mmap_size; ++i)
	{
		const uint64_t base = mmap_array[i].base;
		const uint64_t length = mmap_array[i].length;
		const uint64_t max_addr = base + length;

		if (mmap_array[i].type != TYPE_USABLE)
		{
			kprintf("%d - Base: 0x%x - Length: 0x%x - Type: %d - Skip\n", 
					i, 
					mmap_array[i].base, 
					mmap_array[i],length, 
					mmap_array[i].type);
			continue;
		}

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
		if (base < KERNEL_START && max_addr > KERNEL_START)
		{
			kprintf("Case 1: ");
			// Update the length
			mmap_array[i].length = KERNEL_START - base;
		}
		/* Case 2 */
		else if (base < KERNEL_END && max_addr > KERNEL_END)
		{
			kprintf("Case 2: ");
			// Update the base and length
			mmap_array[i].base = KERNEL_END;
			mmap_array[i].length = max_addr - KERNEL_END;
		}
		/* Case 3 */
		else if (base >= KERNEL_START && max_addr <= KERNEL_END)
		{
			kprintf("Case 3: ");
			// We need to completely remove this region, just mark
			// its type as reserved so we don't use it later
			mmap_array[i].type = TYPE_RESERVED;
		}
		/* Case 4 */
		else if (base < KERNEL_START && max_addr > KERNEL_END)
		{
			kprintf("Case 4: ");
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

		// We need to check a second time because one of the cases above
		// may change the type of the mapping to reserved if it overlaps
		// the kernel
		if (mmap_array[i].type == TYPE_USABLE)
		{
			total_usable_ram += mmap_array[i].length;

			const uint64_t address = mmap_array[i].base + mmap_array[i].length;
			if (address > highest_address)
			{
				highest_address = address;
			}
		}
	}

	if (mmap_size != mmap_add_index) { kprintf("Extras:\n"); }
	// Loop through any added entries as well
	for (uint32_t i = mmap_size; i < mmap_add_index; ++i)
	{
		total_usable_ram += mmap_array[i].length;
		kprintf("%d - Base: 0x%x - Length: 0x%x - Type: %d\n", 
				i, 
				mmap_array[i].base, 
				mmap_array[i].length, 
				mmap_array[i].type);

		const uint64_t address = mmap_array[i].base + mmap_array[i].length;
		if (address > highest_address)
		{
			highest_address = address;
		}
	}

	// Update the mmap_size value
	*((uint32_t*) MMAP_COUNT) = mmap_add_index;

	// Fill in the amount of available ram
	info->total_usable_ram = total_usable_ram;
	info->highest_address = highest_address;
}
