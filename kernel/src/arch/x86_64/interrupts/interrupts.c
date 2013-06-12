#include "interrupts.h"

#include "tss.h"
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


