#ifndef __VIRT_MEMORY_PAGING_H__
#define __VIRT_MEMORY_PAGING_H__

#include "arch/x86_64/virt_memory/types.h"

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
