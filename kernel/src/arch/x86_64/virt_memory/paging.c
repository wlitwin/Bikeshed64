#include "paging.h"
#include "physical.h"
#include "phys_alloc.h"
#include "imports.h"

#include "kernel/klib.h" // memset

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

#define COW_BITS 0x100 // Bit 9

/* How many virtual address bits the processor has
 *
 * Defined in prekernel.s
 */
extern uint8_t processor_virt_bits;

void virt_memory_init()
{
	kernel_table = (void*) &kernel_PML4;	

	/* XXX - Temporarily here for debugging */
	init_serial_debug();	
	init_text_mode();
	clear_screen();

	phys_memory_init();
}

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

uint8_t virt_map_phys(void* _table, const uint64_t virt_addr, const uint64_t phys_addr,
		const uint64_t flags, const uint64_t page_size)
{
	PML4_Table* table = (PML4_Table*) PHYS_TO_VIRT(_table);
	// Calculate the entries
	const uint64_t pml4_index = PML4_INDEX(virt_addr);
	const uint64_t pdpt_index = PDPT_INDEX(virt_addr);
	const uint64_t pdt_index  = PDT_INDEX(virt_addr);
	const uint64_t pt_index   = PT_INDEX(virt_addr);

#ifdef DEBUG_PAGING
	kprintf("PML4: %u\n", pml4_index);
	kprintf("PDPT: %u\n", pdpt_index);
	kprintf("PDT : %u\n", pdt_index);
	kprintf("PT  : %u\n", pt_index);
#endif

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

		memset(pdp_table, 0, sizeof(PDP_Table));

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

		memset(pd_table, 0, sizeof(PD_Table));

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

			memset(p_table, 0, sizeof(P_Table));	

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

void virt_cleanup_table(void* _table)
{
	PML4_Table* table = (PML4_Table*) PHYS_TO_VIRT(_table);
	for (uint64_t pml4_index = 0; pml4_index < 512; ++pml4_index)
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

void* virt_clone_mapping(const void* _other)
{
	PML4_Table* other = (PML4_Table*) PHYS_TO_VIRT(_other);
	PML4_Table* new_table = (PML4_Table*) PHYS_TO_VIRT(phys_alloc_4KIB());

	if (new_table == NULL)
	{
		return NULL;
	}

	memset(new_table, 0, sizeof(PML4_Table));
	/*for (uint64_t pml4_index = 0; pml4_index < 256; ++pml4_index)
	{
		// Set the entries equal, except clear the writable flag
		// and set the copy-on-write bit. The COW has not been fully
		// thought out, so it will most likely change quite a bit.
		const uint64_t entry = other->entries[pml4_index];
		new_table->entries[pml4_index] = (entry & ~(0x2)) | COW_BITS;
	}
	*/

	for (uint64_t pml4_index = 256; pml4_index < 512; ++pml4_index)
	{
		new_table->entries[pml4_index] = other->entries[pml4_index];
	}

	return VIRT_TO_PHYS(new_table);
}
