#ifndef __VIRT_MEMORY_PAGING_H__
#define __VIRT_MEMORY_PAGING_H__

#include "kernel/virt_memory/defs.h"
#include "arch/x86_64/virt_memory/types.h"

#define _1_KIB 1024ULL
#define _2_KIB (2*_1_KIB)
#define _4_KIB (4*_1_KIB)
#define _1_MIB (1024ULL*_1_KIB)
#define _2_MiB (2ULL*_1_MIB)
#define _512_MiB (512ULL*_1_MIB)
#define _1_GIB (1024ULL*_1_MIB)
#define _512_GIB (512ULL*_1_GIB)

#define PDT_PER_PDPT 512ULL
#define PDT_ENTRIES 512ULL
#define PDPT_ENTRIES 512ULL
#define PML4_ENTRIES 512ULL

#define ALIGN_2MIB(X) (((X) & 0xFFFFFFFFFFE00000) + ((((X) & 0x1FFFFF) > 0) * _2_MiB))
#define ALIGN_4KIB(X) (((X) & 0xFFFFFFFFFFFFF000) + ((((X) & 0xFFF) > 0) * _4_KIB))

#define MASK_2MIB(X) (((uint64_t)X) & 0xFFFFFFFFFFE00000)
#define MASK_4KIB(X) (((uint64_t)X) & 0xFFFFFFFFFFFFF000)

#define PT_PRESENT 0x1
#define PT_WRITABLE 0x2

#define PDT_PRESENT 0x1
#define PDT_WRITABLE 0x2
#define PDT_PAGE_SIZE 0x80

#define PDPT_PRESENT 0x1
#define PDPT_WRITABLE 0x2

#define PML4_PRESENT 0x1
#define PML4_WRITABLE 0x2

#define PG_SAFE_FLAGS (PG_FLAG_RW | PG_FLAG_USER | PG_FLAG_PWT | PG_FLAG_PCD | PG_FLAG_XD)

static inline
void virt_switch_page_table(void* page_table)
{
	__asm__ volatile("movq %%rax, %%cr3" : : "a"((uint64_t)page_table));
}

#define invlpg(X) __asm__ volatile("invlpg %0" :: "m" (X))

/* This is defined in prekernel.s
 *
 * It's really a PML4_Table not a void*
 */
extern PML4_Table kernel_PML4;

/* This is defined in prekernel.s
 */
extern PDP_Table kernel_PDPTE;

/* This is defined in prekernel.s
 */
extern PD_Table kernel_PDT;

/* Initialize the virtual memory system, consequently it initializes
 * the physical memory system because that's needed by the virtual
 * memory system for handing out physical mappings.
 *
 * After this initialization KERNEL_PML4 points to the correct address
 * and is a valid paging structure that can be used inside CR3.
 */
void virt_memory_init(void);

/* Creates a mapping from the virtual address to a physical address
 * so that the virtual address will be valid.
 *
 * Parameters:
 *    table - The PML4 table, the top most paging structure
 *    virt_addr - The virtual address to map to
 *    phys_addr - The physical address to map the virtual address to
 *    flags - The permissions for this page
 *    page_size - Whether to map 4KiB or 2MiB to the virtual address
 *
 * Returns:
 *    1 if successfully mapped, 0 if an allocation failed
 */
uint8_t virt_map_phys(void* table, const uint64_t virt_addr, const uint64_t phys_addr,
						const uint64_t flags, const uint64_t page_size);

uint8_t virt_map_phys_range(void* table, const uint64_t virt_addr, const uint64_t phys_addr,
						const uint64_t flags, const uint64_t page_size, const uint64_t num_pages);

/* Similar to virt_map_phys, except it allocates a free physical piece of
 * memory to use for the virtual mapping.
 *
 * Parameters:
 *    table - The PML4 table, the top most paging structure
 *    virt_addr - The virtual address to map
 *    flags - The permissions for this page
 *    page_size - Whether to map 4KiB or 2MiB to this virtual address
 *
 * Returns:
 *    1 if successfully mapped, 0 if an allocation failed
 */
uint8_t virt_map_page(void* table, const uint64_t virt_addr, 
						const uint64_t flags, const uint64_t page_size, 
						uint64_t* phys_addr);
/* Unmap a virtual address
 *
 * Parameters:
 *    table - The PML4 table, the top most paging structure
 *    virt_addr - The virtual address to unmap
 */
void virt_unmap_page(void* table, uint64_t virt_addr);

/* Completely deletes all paging structures in the given hierarchy.
 *
 * Parameters:
 *    table - The PML4 table, the top most paging structure
 */
void virt_cleanup_table(void* table);

/* Resets a page table to a default state. It is not suitable for running
 * processes, but it is suitable for creating a new process context.
 *
 * Parameters:
 *    table - The page table to reset
 */
void virt_reset_table(void* table);

/* Given a page structure, what a virtual address is mapped to.
 *
 * Parameters:
 *    table - The page table to lookup the virt->phys mapping in
 *    virt_addr - The virtual address to find the mapping for
 *    out_phys - The physical address that address is mapped to
 *
 * NOTE: It returns the physical address. If this address needs to be written
 *       to it must be converted to the kernel's identity mapped region first.
 *
 * Returns:
 *    1 if the lookup was successful, 0 if no mapping exists. The out_phys 
 *    parameter is only modified if this function returns 1. It will contain
 *    the physical address that the virt_addr maps to.
 */
uint8_t virt_lookup_phys(void* table, uint64_t virt_addr, uint64_t* out_phys);

/* Clones a PML4 mapping. This does a copy-on-write clone. So
 * only the entry values are copied, but not the pages that are
 * pointed to by the entries. Once an entry is written to then
 * a page fault will occur and the actual copy will happen.
 *
 * Parameters:
 *    other - The PML4 table to create a copy of
 *
 * Returns:
 *    A new PML4 table that has the same mappings as the given one,
 *    or NULL if space for a new PML4 table could not be allocated.
 */
void* virt_clone_mapping(void* other);

#endif
