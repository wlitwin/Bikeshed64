#include "paging.h"
#include "physical.h"
#include "phys_alloc.h"

#include "kernel/klib.h" // memset

#include "arch/x86_64/panic.h"
#include "arch/x86_64/serial.h"
#include "arch/x86_64/textmode.h"

#define PML4_INDEX(X) (((X) >> 38) & 0x7FFFFFFFFF)
#define PDPT_INDEX(X) (((X) >> 29) & 0x3FFFFFFF) 
#define PDT_INDEX(X)  (((X) >> 20) & 0x1FFFFF)

#define PML4E_TO_PDPT(X) (((X) >> 12) & 0x7FFFFFF)
#define PDPTE_TO_PDT(X)  (((X) >> 30) & 0x3FFFFF)

/* How many virtual address bits the processor has
 *
 * Defined in prekernel.s
 */
extern uint8_t processor_virt_bits;

void virt_memory_init()
{
	KERNEL_PML4 = &kernel_PML4;	

	/* XXX - Temporarily here for debugging */
	//init_serial_debug();	
	init_text_mode();

	phys_memory_init();
}

uint8_t virt_map_page(PML4_Table* table, const uint64_t virt_addr, const uint64_t flags)
{
	// Calculate the entries
	const uint64_t pml4_index = PML4_INDEX(virt_addr);
	const uint64_t pdpt_index = PDPT_INDEX(virt_addr);
	const uint64_t pdt_index  = PDT_INDEX(virt_addr);

	if ((table->entries[pml4_index] & PML4_PRESENT) == 0)	
	{
		// We need to allocate
		PDP_Table* pdp_table = (PDP_Table*) phys_alloc_4KIB();
		if (pdp_table == NULL)
		{
			return 0;
		}

		memset(pdp_table, 0, sizeof(PDP_Table));

		// TODO check for flags

		table->entries[pml4_index] = (uint64_t) pdp_table | PML4_PRESENT;
	}

	PDP_Table* pdp_table = (PDP_Table*) PML4E_TO_PDPT(table->entries[pml4_index]);
	if ((pdp_table->entries[pdpt_index] & PDPT_PRESENT) == 0)
	{
		PD_Table* pd_table = (PD_Table*) phys_alloc_4KIB();
		if (pd_table == NULL)
		{
			return 0;
		}

		memset(pd_table, 0, sizeof(PD_Table));

		// TODO check for flags
		
		pdp_table->entries[pdpt_index] = (uint64_t) pd_table | PDPT_PRESENT;
	}

	PD_Table* pd_table = (PD_Table*) PDPTE_TO_PDT(pdp_table->entries[pdpt_index]);

	if ((pd_table->entries[pdt_index] & PDT_PRESENT) > 0)
	{
		panic("Address already mapped!\n");
	}

	pd_table->entries[pdt_index] = (uint64_t) MASK_2MIB(virt_addr) | PDT_PAGE_SIZE | PDT_PRESENT;

	// TODO check for flags
	
	return 1;
}

void virt_unmap_page(PML4_Table* table, uint64_t virt_addr)
{
	// Calculate the entries
	const uint64_t pml4_index = PML4_INDEX(virt_addr);
	const uint64_t pdpt_index = PDPT_INDEX(virt_addr);
	const uint64_t pdt_index  = PDT_INDEX(virt_addr);

	if ((table->entries[pml4_index] & PML4_PRESENT) == 0)
	{
		panic("Can't unmap non-present page");
	}
}

void virt_cleanup_page(PML4_Table* table)
{

}

PML4_Table* virt_clone_mapping(const PML4_Table* other)
{

}
