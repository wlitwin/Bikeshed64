#ifndef __X86_64_VIRT_MEMORY_IMPORTS_H__
#define __X86_64_VIRT_MEMORY_IMPORTS_H__

#define PG_FLAG_RW 0x2
#define PG_FLAG_USER 0x4
#define PG_FLAG_PWT 0x8
#define PG_FLAG_PCD 0x10
#define PG_FLAG_XD 0x8000000000000000

#define PAGE_SMALL 0x1
#define PAGE_LARGE 0x2

#define PAGE_SMALL_SIZE 0x1000
#define PAGE_LARGE_SIZE 0x200000

/* Pointer to the current kernel paging structure
 */
void* kernel_table;

#endif
