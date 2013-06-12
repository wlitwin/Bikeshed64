#include "interrupts.h"

#include "safety.h"
#include "inttypes.h"

#include "kernel/klib.h"

#include "arch/x86_64/panic.h"
#include "arch/x86_64/kprintf.h"

#define IDT_SEG_PRESENT (0x1 << 15)

#define IDT_DPL0 (0x0 << 13)
#define IDT_DPL1 (0x1 << 13)
#define IDT_DPL2 (0x2 << 13)
#define IDT_DPL3 (0x3 << 13)

#define IDT_TYPE_INT_GATE (0xE << 7)

typedef struct
{
	uint16_t offset_L;
	uint16_t segment_selector;
	uint8_t ist;
	uint8_t flags;
	uint16_t offset_H;
	uint32_t offset_HH;
	uint32_t reserved;
} __attribute__((packed)) IDT_Gate;

COMPILE_ASSERT(sizeof(IDT_Gate) == 16);

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

#define GDT_CODE_SEG 0x10
#define GDT_DATA_SEG 0x20

#define idt start_idt_64
extern IDT_Gate start_idt_64[256];

typedef void (*interrupt_handler)(void);

static void set_idt_entry(uint64_t index, interrupt_handler fn_ih)
{
	if (index > 255)
	{
		panic("Bad interrupt handler!");
	}

	const uint64_t fn_loc = (uint64_t)fn_ih;

	idt[index].offset_L = fn_loc & 0xFFFF;
	idt[index].segment_selector = GDT_CODE_SEG;
	idt[index].ist = 0;
	idt[index].flags = 0x8E;
	idt[index].offset_H = (fn_loc >> 16) & 0xFFFF;
	idt[index].offset_HH = (fn_loc >> 32) & 0xFFFFFFFF;
	idt[index].reserved = 0;
}

uint64_t contents = 0x3;

static void setup_tss_descriptor()
{
	const uint64_t tss_base = (uint64_t)&kernel_TSS;
	const uint64_t tss_limit = sizeof(TSS);

	tss_seg_64.limit = tss_limit & 0xFF;
	tss_seg_64.base1 = tss_base & 0xFFFF;
	tss_seg_64.base2 = (tss_base & 0xFF0000) >> 16;
	tss_seg_64.flags = TSS_DESC_P | TSS_DESC_DPL0 | TSS_DESC_TYPE_AVAIL;
	tss_seg_64.limit2 = (tss_limit & 0xF0000) >> 16;
	tss_seg_64.base3 = (tss_base & 0xFF000000) >> 24;
	tss_seg_64.base4 = (tss_base & 0xFFFFFFFF00000000) >> 32;
	tss_seg_64.reserved = 0;

	memset(&kernel_TSS, 0, sizeof(kernel_TSS));
	kernel_TSS.io_map_base = 104;
	kernel_TSS.rsp[0] = 0x80000;
	kernel_TSS.ist[0] = 0x81000;

	// Load this new TSS
	__asm__ volatile ("movw $0x30, %ax");
	__asm__ volatile ("ltr %ax");
}

void handler()
{
	kprintf("Interrupt!\n");
	__asm__("cli");
	__asm__("hlt");
}

void interrupts_init()
{
	setup_tss_descriptor();

	for (int i = 0; i < 256; ++i)
	{
		set_idt_entry(i, &handler);
	}
	
	__asm__ volatile ("sti");
	__asm__ volatile ("int $64");
	__asm__ volatile ("cli");
	__asm__("hlt");
}


