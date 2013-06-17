#ifndef __KERNEL_VIRT_MEMORY_DEFS_H__
#define __KERNEL_VIRT_MEMORY_DEFS_H__

#include "inttypes.h"

// TODO - Include these based on architecture type
#define PG_FLAG_RW 0x2
#define PG_FLAG_USER 0x4
#define PG_FLAG_PWT 0x8
#define PG_FLAG_PCD 0x10
#define PG_FLAG_XD 0x8000000000000000

#define PAGE_SMALL 0x1
#define PAGE_LARGE 0x2

#define PAGE_SMALL_SIZE 0x1000
#define PAGE_LARGE_SIZE 0x200000

extern uint64_t kernel_PML4;

extern void virt_memory_init(void);

extern uint8_t virt_map_phys(void* table, const uint64_t virt_addr, 
						const uint64_t phys_addr, const uint64_t flags, 
						const uint64_t page_size);

extern uint8_t virt_map_page(void* table, const uint64_t virt_addr, 
						const uint64_t flags, const uint64_t page_size);

extern void virt_unmap_page(void* table, uint64_t virt_addr);

extern void virt_cleanup_table(void* table);

extern void* virt_clone_mapping(const void* table);

#endif
