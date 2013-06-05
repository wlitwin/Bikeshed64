#ifndef __VIRT_MEMORY_TYPES_H__
#define __VIRT_MEMORY_TYPES_H__

#include "safety.h"
#include "inttypes.h"

/* PML4 Types. This is the top of the paging hierarchy
 *
 * It contains entries that point to PAE Page-Directory-Pointer-Tables (PDPT)
 *
 * It must fit into a 4KiB block of memory. Since the entries are 64-bits wide
 * then we can only have 512 entries instead of the normal 1024 like 32-bit mode
 */
typedef uint64_t PML4_Entry;
COMPILE_ASSERT(sizeof(PML4_Entry) == 8);

typedef struct
{
	PML4_Entry entries[512];
} PML4_Table;

COMPILE_ASSERT(sizeof(PML4_Table) == 4096);

/* PDPT types. This is the second level of the paging hierarchy
 *
 * It contains entries that map 1GiB pages or point to Page-Directories (PD)
 * that map 2MiB regions (or point to Page Tables).
 *
 * NOTE: Not all processors support 1GiB pages, so this must be checked for 
 *       by using the CPUID instruction.
 *
 * It must also fit into 4KiB and the entries are also 64-bits wide so we can
 * only fit 512 of them in this space.
 */
typedef uint64_t PDPT_Entry;
COMPILE_ASSERT(sizeof(PDPT_Entry) == 8);

typedef struct
{
	PDPT_Entry entries[512];
} PDP_Table;

COMPILE_ASSERT(sizeof(PDP_Table) == 4096);

/* PD types. This is the third level of the paging hierachy
 *
 * It contains entries that map 2MiB regions of memory or point to Page-Tables (PT)
 * that map 4KiB regions of memory.
 *
 * It must also fit into 4KiB and the entries are also 64-bits wide so we can only
 * fit 512 of them in this space.
 */
typedef uint64_t PD_Entry;
COMPILE_ASSERT(sizeof(PD_Entry) == 8);

typedef struct
{
	PD_Entry entries[512];
} PD_Table;

COMPILE_ASSERT(sizeof(PD_Table) == 4096);

/* PT types. This is the fourth and final level of the paging hiearchy
 *
 * It contains entries that map 4KiB regions of memory.
 *
 * It has to fit into 4KiB and it's entries are 64-bits in size so only 512 of
 * them can fit into this space.
 */
typedef uint64_t PT_Entry;
COMPILE_ASSERT(sizeof(PT_Entry) == 8);

typedef struct
{
	PT_Entry entries[512];
} P_Table;

COMPILE_ASSERT(sizeof(P_Table) == 4096);

#endif
