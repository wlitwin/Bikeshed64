#include "tss.h"

#include "safety.h"
#include "kernel/klib.h" // memclr
#include "arch/x86_64/virt_memory/physical.h"

typedef struct
{
	uint16_t limit;
	uint16_t base1;
	uint8_t base2;
	uint8_t flags;
	uint8_t limit2;
	uint8_t base3;
	uint32_t base4;
	uint32_t reserved;
} __attribute__((packed)) TSS_Descriptor;

COMPILE_ASSERT(sizeof(TSS_Descriptor) == 16);

typedef struct
{
	uint32_t reserved1;
	uint64_t rsp[3];
	uint64_t reserved2;
	uint64_t ist[7];
	uint64_t reserved3;
	uint16_t reserved4;
	uint16_t io_map_base;
} __attribute__((packed)) TSS;

COMPILE_ASSERT(sizeof(TSS) == 104);

static TSS kernel_TSS;

#define TSS_DESC_DPL0 (0x00 << 1)
#define TSS_DESC_DPL1 (0x01 << 1)
#define TSS_DESC_DPL2 (0x02 << 1)
#define TSS_DESC_DPL3 (0x03 << 1)

#define TSS_DESC_P 0x80
#define TSS_DESC_TYPE_AVAIL 0x9

extern TSS_Descriptor tss_seg_64;
TSS_Descriptor* tss;

static
void setup_kernel_tss()
{
	// Setup the kernel TSS
	const uint64_t tss_base = (uint64_t)&kernel_TSS;
	const uint64_t tss_limit = sizeof(TSS);

	tss = PHYS_TO_VIRT(&tss_seg_64);

	tss->limit = tss_limit & 0xFF;
	tss->base1 = tss_base & 0xFFFF;
	tss->base2 = (tss_base & 0xFF0000) >> 16;
	tss->flags = TSS_DESC_P | TSS_DESC_DPL0 | TSS_DESC_TYPE_AVAIL;
	tss->limit2 = (tss_limit & 0xF0000) >> 16;
	tss->base3 = (tss_base & 0xFF000000) >> 24;
	tss->base4 = (tss_base & 0xFFFFFFFF00000000) >> 32;
	tss->reserved = 0;

	memclr(&kernel_TSS, sizeof(kernel_TSS));
	kernel_TSS.io_map_base = 104;
	kernel_TSS.rsp[0] = KERNEL_STACK_LOCATION;
	kernel_TSS.ist[0] = KERNEL_STACK_LOCATION;
}

void tss_set_context_stack(const uint64_t location)
{
	kernel_TSS.rsp[0] = location;
}

void setup_tss_descriptor()
{
	setup_kernel_tss();

	// Load this new TSS
	__asm__ volatile ("movw $" SX(TSS_SEG_64)  ", %ax");
	__asm__ volatile ("ltr %ax");
}
