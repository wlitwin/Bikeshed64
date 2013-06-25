#ifndef __X86_64_VIRT_MEMORY_PHYS_ALLOC_H__
#define __X86_64_VIRT_MEMORY_PHYS_ALLOC_H__

void setup_physical_allocator(void);

void* phys_alloc_2MIB(void);

void* phys_alloc_2MIB_safe(const char* error);

void phys_free_2MIB(void* ptr);

void* phys_alloc_4KIB(void);

void* phys_alloc_4KIB_safe(const char* error);

void phys_free_4KIB(void* ptr);

#endif
