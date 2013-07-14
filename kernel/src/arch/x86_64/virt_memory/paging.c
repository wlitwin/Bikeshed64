#include "paging.h"
#include "physical.h"
#include "phys_alloc.h"
#include "imports.h"

#include "kernel/klib.h" // memclr

#include "arch/x86_64/panic.h"
#include "arch/x86_64/serial.h"
#include "arch/x86_64/kprintf.h"
#include "arch/x86_64/textmode.h"

#define ENTRY_TO_ADDR(X) ((X) & 0x7FFFFFFFFFFFF000)

#define PML4_INDEX(X) (((X) >> 39) & 0x1FF)
#define PDPT_INDEX(X) (((X) >> 30) & 0x1FF) 
#define PDT_INDEX(X)  (((X) >> 21) & 0x1FF)
#define PT_INDEX(X)   (((X) >> 12) & 0x1FF)

#define PML4E_TO_PDPT(X) ((PDP_Table*)((X) & 0x7FFFFFF000))
#define PDPTE_TO_PDT(X)  ((PD_Table*) ((X) & 0x7FFFFFF000))
#define PDTE_TO_PT(X)    ((P_Table*)  ((X) & 0x7FFFFFF000))

#define PAGE_COPY_FLAGS (PG_SAFE_FLAGS | 0x1)

// Copy on write macros
#define PAGE_COW 0x100 // Bit 9
#define PAGE_COW_WR 0x200 // Bit 10
#define PAGE_COW_REF 0x7FF
#define PAGE_IS_COW(X) (((X) & PAGE_COW) > 0)
#define PAGE_GET_REF(X) (((X) >> 52) & PAGE_COW_REF)
#define PAGE_SET_REF(PG,VAL) ((PG) | (((VAL) & PAGE_COW_REF) << 52))
#define PAGE_COW_INC(PG) PAGE_SET_REF(PG,(PAGE_GET_REF(PG))+1)
#define PAGE_COW_DEC(PG) PAGE_SET_REF(PG,(PAGE_GET_REF(PG))-1)

#ifndef DEBUG_VIRT_MEM
#define kprintf(...)
#endif

/* How many virtual address bits the processor has
 *
 * Defined in prekernel.s
 */
extern uint8_t processor_virt_bits;

void* kernel_table;

//=============================================================================
//
//=============================================================================

void virt_memory_init()
{
	kernel_table = (void*) &kernel_PML4;	

#ifdef QEMU
	/* XXX - Temporarily here for debugging */
	init_serial_debug();	
#endif
	init_text_mode();
	clear_screen();

	phys_memory_init();

	// TODO - hack, user processes want to access VGA memory, but right now
	//        any good way requires some changes. For example if we map the
	//        actual physical address, then virt_clone_mapping() is broken
	//        because it assumes all mappings are backed by some allocated
	//        space, when instead we want a dumb pass-through.
	virt_map_phys(kernel_table, 0xFFFFFFFFFFBFF000, 0xB8000, 
			PG_FLAG_RW | PG_FLAG_USER | PG_FLAG_PWT | PG_FLAG_PCD, PAGE_SMALL);
}

//=============================================================================
//
//=============================================================================

uint8_t virt_map_page(void* _table, const uint64_t virt_addr,
		const uint64_t flags, const uint64_t page_size,
		uint64_t* phys_addr)
{
	uint64_t addr = 0;
	if (page_size == PAGE_LARGE)
	{
		addr = (uint64_t) phys_alloc_2MIB();
		if (addr == 0)
		{
			kprintf("virt_map_page: Failed to allocate 2MiB\n");
		}
	}
	else if (page_size == PAGE_SMALL)
	{
		addr = (uint64_t) phys_alloc_4KIB();
		if (addr == 0)
		{
			kprintf("virt_map_page: Failed to allocate 4KiB\n");
		}
	}
	else
	{
		panic("virt_map_page: Invalid page size\n");
	}

	if (phys_addr != NULL)
	{
		*phys_addr = addr;
	}

	if (addr == 0)
	{
		return 0;
	}

	return virt_map_phys(_table, virt_addr, addr, flags, page_size);
}
//=============================================================================
//
//=============================================================================

uint8_t virt_map_phys_range(void* table, const uint64_t virt_addr, const uint64_t phys_addr,
						const uint64_t flags, const uint64_t page_size, const uint64_t num_pages)
{
	ASSERT(page_size == PAGE_LARGE || page_size == PAGE_SMALL);

	uint64_t cur_virt = virt_addr;
	uint64_t cur_phys = phys_addr;
	const uint64_t addr_inc = page_size == PAGE_LARGE ? PAGE_LARGE_SIZE : PAGE_SMALL_SIZE;

	for (uint64_t i = 0; i < num_pages; ++i)
	{
		const uint8_t retVal = virt_map_phys(table, cur_virt, cur_phys, flags, page_size);
		if (!retVal) 
		{
			return retVal;
		}

		cur_virt += addr_inc;
		cur_phys += addr_inc;
	}

	return 0;
}

//=============================================================================
//
//=============================================================================

uint8_t virt_map_phys(void* _table, const uint64_t virt_addr, const uint64_t phys_addr,
		const uint64_t flags, const uint64_t page_size)
{
	PML4_Table* table = (PML4_Table*) PHYS_TO_VIRT(_table);
	// Calculate the entries
	const uint64_t pml4_index = PML4_INDEX(virt_addr);
	const uint64_t pdpt_index = PDPT_INDEX(virt_addr);
	const uint64_t pdt_index  = PDT_INDEX(virt_addr);
	const uint64_t pt_index   = PT_INDEX(virt_addr);

	kprintf("PML4: %u\n", pml4_index);
	kprintf("PDPT: %u\n", pdpt_index);
	kprintf("PDT : %u\n", pdt_index);
	kprintf("PT  : %u\n", pt_index);

	const uint64_t safe_flags = flags & 
		(PG_FLAG_RW | PG_FLAG_USER | PG_FLAG_PWT | PG_FLAG_PCD | PG_FLAG_XD);

	if ((table->entries[pml4_index] & PML4_PRESENT) == 0)	
	{
		// We need to allocate
		PDP_Table* pdp_table = (PDP_Table*) PHYS_TO_VIRT(phys_alloc_4KIB());
		if (pdp_table == NULL)
		{
			return 0;
		}

		memclr(pdp_table, sizeof(PDP_Table));

		table->entries[pml4_index] = (uint64_t) VIRT_TO_PHYS(pdp_table) | PML4_WRITABLE | PML4_PRESENT;
		invlpg(pdp_table);
	}

	PDP_Table* pdp_table = PHYS_TO_VIRT(PML4E_TO_PDPT(table->entries[pml4_index]));
	if ((pdp_table->entries[pdpt_index] & PDPT_PRESENT) == 0)
	{
		PD_Table* pd_table = (PD_Table*) PHYS_TO_VIRT(phys_alloc_4KIB());
		if (pd_table == NULL)
		{
			return 0;
		}

		memclr(pd_table, sizeof(PD_Table));

		pdp_table->entries[pdpt_index] = (uint64_t) VIRT_TO_PHYS(pd_table) | PDPT_WRITABLE | PDPT_PRESENT;
		invlpg(pd_table);
	}

	PD_Table* pd_table = PHYS_TO_VIRT(PDPTE_TO_PDT(pdp_table->entries[pdpt_index]));

	// Check if something is already mapped here
	if ((pd_table->entries[pdt_index] & PDT_PAGE_SIZE) > 0 &&
		(pd_table->entries[pdt_index] & PDT_PRESENT) > 0)
	{
		kprintf("Vaddr: 0x%x - Paddr: 0x%x\n", virt_addr, phys_addr);
		kprintf("PD Mapped to: 0x%x\n", pd_table->entries[pdt_index]);
		panic("Address already mapped!\n");
	}

	if (page_size == PAGE_LARGE)
	{
		pd_table->entries[pdt_index] = (uint64_t)MASK_2MIB(phys_addr) | PDT_PAGE_SIZE | PDT_PRESENT;
		pd_table->entries[pdt_index] |= safe_flags;
	}
	else if (page_size == PAGE_SMALL)
	{
		if ((pd_table->entries[pdt_index] & PDT_PRESENT) == 0)
		{
			// Allocate some space for it
			P_Table* p_table = (P_Table*) PHYS_TO_VIRT(phys_alloc_4KIB());
			if (p_table == NULL)
			{
				return 0;
			}

			memclr(p_table, sizeof(P_Table));	

			pd_table->entries[pdt_index] = (uint64_t)VIRT_TO_PHYS(p_table) | PDT_WRITABLE | PDT_PRESENT;
			invlpg(p_table);
		}

		P_Table* p_table = PHYS_TO_VIRT(PDTE_TO_PT(pd_table->entries[pdt_index]));

		// Check if something is already mapped here
		if ((p_table->entries[pt_index] & PT_PRESENT) > 0)
		{
			kprintf("Vaddr: 0x%x - Paddr: 0x%x\n", virt_addr, phys_addr);
			kprintf("PT Mapped to: 0x%x\n", p_table->entries[pt_index]);
			panic("Address already mapped!\n");
		}

		p_table->entries[pt_index] = (uint64_t)MASK_4KIB(phys_addr) | PT_PRESENT;
		p_table->entries[pt_index] |= safe_flags;
	}
	else
	{
		panic("virt_map_page: Invalid page size specified");
	}

	invlpg(virt_addr);

	return 1;
}

//=============================================================================
//
//=============================================================================

void virt_unmap_page(void* _table, uint64_t virt_addr)
{
	PML4_Table* table = (PML4_Table*) PHYS_TO_VIRT(_table);
	// TODO what if a cloned PML4 table has been created, and then
	// this method called?

	// Calculate the entries
	const uint64_t pml4_index = PML4_INDEX(virt_addr);
	const uint64_t pdpt_index = PDPT_INDEX(virt_addr);
	const uint64_t pdt_index  = PDT_INDEX(virt_addr);

	if ((table->entries[pml4_index] & PML4_PRESENT) == 0)
	{
		panic("Can't unmap non-present page (1)");
	}

	PDP_Table* pdp_table = PHYS_TO_VIRT(PML4E_TO_PDPT(table->entries[pml4_index]));
	if ((pdp_table->entries[pdpt_index] & PDPT_PRESENT) == 0)
	{
		panic("Can't unmap non-present page (2)");
	}

	PD_Table* pd_table = PHYS_TO_VIRT(PDPTE_TO_PDT(pdp_table->entries[pdpt_index]));
	if ((pd_table->entries[pdt_index] & PDT_PRESENT) == 0)
	{
		panic("Can't unmap non-present page (3)");
	}

	if ((pd_table->entries[pdt_index] & PDT_PAGE_SIZE) > 0)
	{
		// 2MiB region, mask the address
		const uint64_t address = ENTRY_TO_ADDR(pd_table->entries[pdt_index]);
		phys_free_2MIB((void*)address);

		pd_table->entries[pdt_index] = 0;
	}
	else
	{
		const uint64_t pt_index = PT_INDEX(virt_addr);

		// Get the page table, 4KiB region
		P_Table* p_table = PHYS_TO_VIRT(PDTE_TO_PT(pd_table->entries[pdt_index]));
		if ((p_table->entries[pt_index] & PT_PRESENT) == 0)
		{
			panic("Can't unmap non-present page (4)");
		}

		// Unmap the page
		const uint64_t address = ENTRY_TO_ADDR(p_table->entries[pt_index]);
		phys_free_4KIB((void*)address);
		p_table->entries[pt_index] = 0;
	}

	// TODO could check if table is completely empty and then free the whole thing
	// TODO invlpg
}

//=============================================================================
//
//=============================================================================

static void cleanup_page_table(P_Table* p_table)
{
	for (uint64_t pt_index = 0; pt_index < 512; ++pt_index)
	{
		const uint64_t entry = p_table->entries[pt_index];
		if ((entry & PT_PRESENT) > 0)
		{
			const uint64_t address = ENTRY_TO_ADDR(entry);
			phys_free_4KIB((void*)address);
		}
	}

	// Cleanup the actual page table
	kprintf("Cleaning page table: 0x%x\n", p_table);

	phys_free_4KIB(VIRT_TO_PHYS(p_table));
}

//=============================================================================
//
//=============================================================================

static void cleanup_page_directory_table(PD_Table* pd_table)
{
	for (uint64_t pdt_index = 0; pdt_index < 512; ++pdt_index)
	{
		const uint64_t entry = pd_table->entries[pdt_index];
		if ((entry & PDT_PRESENT) > 0)
		{
			if ((entry & PDT_PAGE_SIZE) > 0)
			{
				const uint64_t address = ENTRY_TO_ADDR(entry);
				phys_free_2MIB((void*)address);
			}
			else
			{
				cleanup_page_table(PHYS_TO_VIRT(PDTE_TO_PT(entry)));
			}
		}
	}

	// Cleanup the actual page directory
	kprintf("Cleaning page directory table: 0x%x\n", pd_table);
	phys_free_4KIB(VIRT_TO_PHYS(pd_table));
}

//=============================================================================
//
//=============================================================================

static void cleanup_page_directory_pointer_table(PDP_Table* pdp_table)
{
	for (uint64_t pdpt_index = 0; pdpt_index < 512; ++pdpt_index)
	{
		const uint64_t entry = pdp_table->entries[pdpt_index];
		if ((entry & PDPT_PRESENT) > 0)
		{
			cleanup_page_directory_table(PHYS_TO_VIRT(PDPTE_TO_PDT(entry)));
		}
	}

	// Cleanup the actual page directory pointer table
	kprintf("Cleaning page directory pointer table: 0x%x\n", pdp_table);
	phys_free_4KIB(VIRT_TO_PHYS(pdp_table));
}

//=============================================================================
//
//=============================================================================

void virt_cleanup_table(void* _table)
{
	PML4_Table* table = (PML4_Table*) PHYS_TO_VIRT(_table);
	for (uint64_t pml4_index = 0; pml4_index < 256; ++pml4_index)
	{
		const uint64_t entry = table->entries[pml4_index];
		if ((entry & PML4_PRESENT) > 0)
		{
			cleanup_page_directory_pointer_table(PHYS_TO_VIRT(PML4E_TO_PDPT(entry)));
		}
	}

	// Cleanup the PML4 table
	phys_free_4KIB(_table);
}

//=============================================================================
//
//=============================================================================

void virt_reset_table(void* table)
{
	if (table == kernel_table)
	{
		panic("virt_reset_table: Bad param - kernel's page table");
	}

	// Index 256 marks the start of the kernel's address space
	PML4_Table* pml4_table = (PML4_Table*) PHYS_TO_VIRT(table);
	for (uint64_t pml4_index = 0; pml4_index < 256; ++pml4_index)
	{
		const uint64_t entry = pml4_table->entries[pml4_index];
		if ((entry & PML4_PRESENT) > 0)
		{
			cleanup_page_directory_pointer_table(PHYS_TO_VIRT(PML4E_TO_PDPT(entry)));
		}
		pml4_table->entries[pml4_index] = 0;
	}
}

//=============================================================================
//
//=============================================================================

uint8_t virt_lookup_phys(void* table, uint64_t virt_addr, uint64_t* out_phys)
{
	// Calculate all of the offsets
	const uint64_t pml4_index = PML4_INDEX(virt_addr);
	const uint64_t pdpt_index = PDPT_INDEX(virt_addr);
	const uint64_t pdt_index  = PDT_INDEX(virt_addr);
	const uint64_t pt_index   = PT_INDEX(virt_addr);

	kprintf("PML4: %u\n", pml4_index);
	kprintf("PDPT: %u\n", pdpt_index);
	kprintf("PDT : %u\n", pdt_index);
	kprintf("PT  : %u\n", pt_index);
	kprintf("Virt: 0x%x\n", virt_addr);

	PML4_Table* pml4_table = (PML4_Table*) PHYS_TO_VIRT(table);

	const uint64_t pml4_entry = pml4_table->entries[pml4_index];
	if ((pml4_entry & PML4_PRESENT) > 0)
	{
		kprintf("PML4: 0x%x\n", pml4_entry);
		PDP_Table* pdp_table = PHYS_TO_VIRT(PML4E_TO_PDPT(pml4_entry));
		const uint64_t pdpt_entry = pdp_table->entries[pdpt_index];

		if ((pdpt_entry & PDPT_PRESENT) > 0)
		{
			kprintf("PDPT: 0x%x\n", pdpt_entry);
			PD_Table* pd_table = PHYS_TO_VIRT(PDPTE_TO_PDT(pdpt_entry));
			const uint64_t pdt_entry = pd_table->entries[pdt_index];

			if ((pdt_entry & PDT_PRESENT) > 0)
			{
				kprintf("PDT: 0x%x\n", pdt_entry);
				uint64_t out_address = 0;
				if ((pdt_entry & PDT_PAGE_SIZE) > 0)
				{
					kprintf("PAGE_LARGE\n");
					out_address =
						ENTRY_TO_ADDR(pdt_entry) + (virt_addr & 0xFFF);
				}
				else
				{
					P_Table* p_table = PHYS_TO_VIRT(PDTE_TO_PT(pdt_entry));
					const uint64_t pt_entry = p_table->entries[pt_index];
				
					if ((pt_entry & PT_PRESENT) > 0)
					{
						kprintf("PT: 0x%x - 0x%x \n", pt_entry, ENTRY_TO_ADDR(pt_entry));
						out_address =
							ENTRY_TO_ADDR(pt_entry) + (virt_addr & 0xFFF);
						kprintf("OUT ADDRESS: 0x%x\n", out_address);
					}
				}

				*out_phys = out_address;
				return 1;
			}
		}
	}

	// No mapping found
	return 0;
}

//=============================================================================
//
//=============================================================================

static uint64_t clone_page_table(P_Table* p_table)
{
	const char* error = "clone_page_table: No memory";
	const char* error2 = "clone_page_table: No memory (loop)";
	P_Table* new_table = (P_Table*) PHYS_TO_VIRT(phys_alloc_4KIB_safe(error));

	for (uint32_t i = 0; i < 512; ++i)
	{
		const uint64_t entry = p_table->entries[i];
		if ((entry & PT_PRESENT) > 0)
		{
			void* dst = PHYS_TO_VIRT(phys_alloc_4KIB_safe(error2));
			void* src = PHYS_TO_VIRT(ENTRY_TO_ADDR(entry));

			memcpy(dst, src, PAGE_SMALL_SIZE);

			new_table->entries[i] = (uint64_t)VIRT_TO_PHYS(dst) | (entry & PAGE_COPY_FLAGS);
		}
		else
		{
			new_table->entries[i] = 0;
		}
	}

	uint64_t retVal = (uint64_t) VIRT_TO_PHYS(new_table);
	retVal |= (uint64_t)VIRT_TO_PHYS(p_table) & PAGE_COPY_FLAGS;
	return retVal;
}

//=============================================================================
//
//=============================================================================

static uint64_t clone_page_directory(PD_Table* pd_table)
{
	const char* error = "clone_page_directory: No memory";
	const char* error_2MIB = "clone_page_directory: Failed to alloc 2MIB";
	PD_Table* new_table = (PD_Table*) PHYS_TO_VIRT(phys_alloc_4KIB_safe(error));

	for (uint32_t i = 0; i < 512; ++i)
	{
		const uint64_t entry = pd_table->entries[i];	
		if ((entry & PDT_PRESENT) > 0)
		{
			if ((entry & PDT_PAGE_SIZE) > 0)
			{
				// Allocate a 2MIB piece of ram to copy this to	
				void* dst = PHYS_TO_VIRT(phys_alloc_2MIB_safe(error_2MIB));
				void* src = PHYS_TO_VIRT(ENTRY_TO_ADDR(entry));

				memcpy(dst, src, PAGE_LARGE_SIZE);

				new_table->entries[i] = 
					(uint64_t)VIRT_TO_PHYS(dst) | 
						(entry & PAGE_COPY_FLAGS) | PDT_PAGE_SIZE;
			}
			else
			{
				new_table->entries[i] = 
					clone_page_table(PHYS_TO_VIRT(
								PDTE_TO_PT(entry)));
				new_table->entries[i] |= (entry & PAGE_COPY_FLAGS);
			}
		}
		else
		{
			new_table->entries[i] = 0;
		}
	}

	uint64_t retVal = (uint64_t) VIRT_TO_PHYS(new_table);
	retVal |= (uint64_t)VIRT_TO_PHYS(pd_table) & PAGE_COPY_FLAGS;
	return retVal;
}

//=============================================================================
//
//=============================================================================

static uint64_t clone_page_directory_pointer(PDP_Table* pdp_table)
{
	const char* error = "clone_page_directory_pointer: No memory";
	PDP_Table* new_table = (PDP_Table*) PHYS_TO_VIRT(phys_alloc_4KIB_safe(error));

	for (uint32_t i = 0; i < 512; ++i)
	{
		const uint64_t entry = pdp_table->entries[i];
		if ((entry & PDPT_PRESENT) > 0)
		{
			new_table->entries[i] =
				clone_page_directory(PHYS_TO_VIRT(
							PDPTE_TO_PDT(entry)));
			new_table->entries[i] |= (entry & PAGE_COPY_FLAGS);
		}
		else
		{
			new_table->entries[i] = 0;
		}
	}

	uint64_t retVal = (uint64_t)VIRT_TO_PHYS(new_table);

	// Copy the flags
	retVal |= (uint64_t)VIRT_TO_PHYS(pdp_table) & PAGE_COPY_FLAGS;

	return retVal;
}

//=============================================================================
//
//=============================================================================

void* virt_clone_mapping(void* _other)
{
	kprintf("Clone mapping\n");
	PML4_Table* other = (PML4_Table*) PHYS_TO_VIRT(_other);	

	const char* error = "virt_clone_mapping: No memory";
	PML4_Table* new_table = (PML4_Table*) PHYS_TO_VIRT(phys_alloc_4KIB_safe(error));
	// TODO - don't need
	memclr(new_table, sizeof(PML4_Table));

	// Only do COW for the user pages. Kernel pages are shared no matter what
/*	for (uint32_t i = 0; i < 256; ++i)
	{
		const uint64_t o_entry = other->entries[i];
		if ((o_entry & PML4_PRESENT) > 0)
		{
			// We need to turn this into a COW page	
			uint64_t cow_entry = PAGE_SET_REF(o_entry | PAGE_COW, 1);
			// Store if the page was originally writable
			if ((cow_entry & PML4_WRITABLE) > 0)
			{
				cow_entry |= PAGE_COW_WR;
			}
			// We set the page to read-only so we get a fault if one of
			// the processes tries to access the page. We can then create
			// a copy of that page if it was originally writable. If not,
			// then we can issue some kind of protection violation and 
			// kill the offending process.
			cow_entry &= ~PML4_WRITABLE;

			// Both tables get the same value
			other->entries[i] = cow_entry;
			new_table->entries[i] = cow_entry;
		}
	}
	*/
	// We'll implement COW later
	for (uint32_t i = 0; i < 256; ++i)
	{
		const uint64_t entry = other->entries[i];	
		if ((entry & PML4_PRESENT) > 0)
		{
			kprintf("Clone present: 0x%x %u\n", entry, i);
			new_table->entries[i] =
				clone_page_directory_pointer(PHYS_TO_VIRT(
						PML4E_TO_PDPT(entry)));	
			new_table->entries[i] |= (entry & PAGE_COPY_FLAGS);
			kprintf("Clone entry: 0x%x\n", new_table->entries[i]);
			kprintf("Old present: 0x%x\n", other->entries[i]);
		}
		else
		{
			new_table->entries[i] = 0;
		}
	}

	// Copy, but don't modify the kernel's pages
	for (uint64_t pml4_index = 256; pml4_index < 512; ++pml4_index)
	{
		new_table->entries[pml4_index] = other->entries[pml4_index];
	}

	return VIRT_TO_PHYS(new_table);
}

//=============================================================================
//
//=============================================================================
