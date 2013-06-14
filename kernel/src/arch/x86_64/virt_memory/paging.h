#ifndef __VIRT_MEMORY_PAGING_H__
#define __VIRT_MEMORY_PAGING_H__

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

#define PG_FLAG_RW 0x2
#define PG_FLAG_USER 0x4
#define PG_FLAG_PWT 0x8
#define PG_FLAG_PCD 0x10
#define PG_FLAG_XD 0x8000000000000000

#define PAGE_SMALL 0x1
#define PAGE_LARGE 0x2

#define PT_PRESENT 0x1
#define PT_WRITABLE 0x2

#define PDT_PRESENT 0x1
#define PDT_WRITABLE 0x2
#define PDT_PAGE_SIZE 0x80

#define PDPT_PRESENT 0x1
#define PDPT_WRITABLE 0x2

#define PML4_PRESENT 0x1
#define PML4_WRITABLE 0x2

#define invlpg(X) __asm__ volatile("invlpg %0" :: "m" (X))

/* This is defined in prekernel.s
 */
extern PML4_Table kernel_PML4;

/* This is defined in prekernel.s
 */
extern PDP_Table kernel_PDPTE;

/* This is defined in prekernel.s
 */
extern PD_Table kernel_PDT;

/* Pointer to the current kernel PML4
 */
PML4_Table* KERNEL_PML4;

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
uint8_t virt_map_phys(PML4_Table* table, const uint64_t virt_addr, const uint64_t phys_addr,
						const uint64_t flags, const uint64_t page_size);

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
uint8_t virt_map_page(PML4_Table* table, const uint64_t virt_addr, 
						const uint64_t flags, const uint64_t page_size);
/* Unmap a virtual address
 *
 * Parameters:
 *    table - The PML4 table, the top most paging structure
 *    virt_addr - The virtual address to unmap
 */
void virt_unmap_page(PML4_Table* table, uint64_t virt_addr);

/* Completely deletes all paging structures in the given hierarchy.
 *
 * Parameters:
 *    table - The PML4 table, the top most paging structure
 */
void virt_cleanup_table(PML4_Table* table);

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
PML4_Table* virt_clone_mapping(const PML4_Table* other);

#endif
