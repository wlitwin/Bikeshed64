#include "interrupts.h"

#include "tss.h"
#include "apic.h"
#include "safety.h"
#include "inttypes.h"

#include "kernel/klib.h"

#include "arch/x86_64/panic.h"
#include "arch/x86_64/kprintf.h"
#include "arch/x86_64/support.h"

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

void (*isr_table[256])(uint64_t vector, uint64_t code);

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

static void default_handler(uint64_t vector, uint64_t code)
{
	kprintf("Interrupt! vector: %u - Code: %u \n", vector, code);
}

void interrupt_install_isr(uint64_t index, void handler(uint64_t, uint64_t))
{
	isr_table[index] = handler;
}

static void pic_init()
{
	#define PIC_MASTER_CMD_PORT 0x20
	#define PIC_MASTER_IMR_PORT 0x21
	#define PIC_SLAVE_CMD_PORT 0xA0
	#define PIC_SLAVE_IMR_PORT 0xA1
	#define PIC_MASTER_SLAVE_LINE 0x04
	#define PIC_SLAVE_ID 0x02
	#define PIC_86MODE 0x1
	#define PIC_ICW1BASE 0x10
	#define PIC_NEEDICW4 0x01

	// ICW1
	_outb(PIC_MASTER_CMD_PORT, PIC_ICW1BASE | PIC_NEEDICW4);		
	_outb(PIC_SLAVE_CMD_PORT, PIC_ICW1BASE | PIC_NEEDICW4);

	// ICW2
	// Master offset to 32
	// Slave offset to 40
	_outb(PIC_MASTER_IMR_PORT, 0x20);
	_outb(PIC_SLAVE_IMR_PORT, 0x28);

	// ICW3
	_outb(PIC_MASTER_IMR_PORT, PIC_MASTER_SLAVE_LINE);
	_outb(PIC_SLAVE_IMR_PORT, PIC_SLAVE_ID);

	// ICW4
	_outb(PIC_MASTER_IMR_PORT, PIC_86MODE);
	_outb(PIC_SLAVE_IMR_PORT, PIC_86MODE);

	// OCW1 Allow interrupts on all lines
	_outb(PIC_MASTER_IMR_PORT, 0x0);
	_outb(PIC_SLAVE_IMR_PORT, 0x0);
}

void interrupts_init()
{
	setup_tss_descriptor();

	extern interrupt_handler isr_stub_table[256];

	for (uint64_t i = 0; i < 256; ++i)
	{
		set_idt_entry(i, isr_stub_table[i]);
		interrupt_install_isr(i, &default_handler);
	}

	// Initialize the APIC so interrupts from it don't
	// come to the exception interrupt vectors. Currently
	// just disabled the APIC.
	apic_init();

	// For now we'll use the old PIC
	pic_init();	
}


