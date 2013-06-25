#ifndef __KERNEL_VIRT_MEMORY_DEFS_H__
#define __KERNEL_VIRT_MEMORY_DEFS_H__

#include "inttypes.h"

#ifdef BIKESHED_X86_64
#include "arch/x86_64/virt_memory/imports.h"
#endif

extern void* kernel_table;

extern void virt_memory_init(void);

extern uint8_t virt_map_phys(void* table, const uint64_t virt_addr, 
						const uint64_t phys_addr, const uint64_t flags, 
						const uint64_t page_size);

extern uint8_t virt_map_page(void* table, const uint64_t virt_addr, 
						const uint64_t flags, const uint64_t page_size,
						uint64_t* phys_addr);

extern uint8_t virt_lookup_phys(void* table, uint64_t virt_addr, uint64_t* out_phys);

extern void virt_unmap_page(void* table, uint64_t virt_addr);

extern void virt_reset_table(void* table);

extern void virt_cleanup_table(void* table);

extern void* virt_clone_mapping(void* table);

#endif
