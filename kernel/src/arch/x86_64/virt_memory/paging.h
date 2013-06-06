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

#define MASK_2MIB(X) ((X) & 0xFFFFFFFFFFE00000)
#define MASK_4KIB(X) ((X) & 0xFFFFFFFFFFFFF000)

#define PDT_PRESENT 0x1
#define PDT_WRITABLE 0x2
#define PDT_PAGE_SIZE 0x80

#define PDPT_PRESENT 0x1
#define PDPT_WRITABLE 0x2

#define PML4_PRESENT 0x1
#define PML4_WRITABLE 0x2

#define invlpg(X) __asm__ volatile("invlpg %0" ::"m" (X))

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

#endif
