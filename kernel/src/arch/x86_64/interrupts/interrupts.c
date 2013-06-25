#include "interrupts.h"

#include "tss.h"
#include "apic.h"
#include "safety.h"
#include "imports.h"

#include "kernel/klib.h"

#include "arch/x86_64/panic.h"
#include "arch/x86_64/kprintf.h"
#include "arch/x86_64/virt_memory/physical.h"
#include "kernel/scheduler/pcb.h"

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

extern IDT_Gate start_idt_64[256];
IDT_Gate* idt;

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

static void dump_context(Context* context)
{
	kprintf("CONTEXT DUMP:\n");
	kprintf("RDI: 0x%x\n", context->rdi);
	kprintf("RSI: 0x%x\n", context->rsi);
	kprintf("RAX: 0x%x\n", context->rax);
	kprintf("RBX: 0x%x\n", context->rbx);
	kprintf("RCX: 0x%x\n", context->rcx);
	kprintf("RDX: 0x%x\n", context->rdx);
	kprintf("rbp: 0x%x\n", context->rbp);
	kprintf("r8 : 0x%x\n", context->r8);
	kprintf("r9 : 0x%x\n", context->r9);
	kprintf("r10: 0x%x\n", context->r10);
	kprintf("r11: 0x%x\n", context->r11);
	kprintf("r12: 0x%x\n", context->r12);
	kprintf("r13: 0x%x\n", context->r13);
	kprintf("r14: 0x%x\n", context->r14);
	kprintf("r15: 0x%x\n", context->r15);
	kprintf("vec: 0x%x\n", context->vector);
	kprintf("err: 0x%x\n", context->error_code);
	kprintf("rip: 0x%x\n", context->rip);
	kprintf("cs : 0x%x\n", context->cs);
	kprintf("rfl: 0x%x\n", context->rflags);
	kprintf("rsp: 0x%x\n", context->rsp);
	kprintf("ss : 0x%x\n", context->ss);
}

static void default_handler(uint64_t vector, uint64_t code)
{
	extern PCB* current_pcb;

	kprintf("Interrupt! vector: %u - Code: %u \n", vector, code);
	uint64_t context_addr = (uint64_t)current_pcb->context;
	Context* context = (Context*)context_addr;
	kprintf(" PCB: 0x%x\n", current_pcb);
	kprintf(" Context: 0x%x\n", context);
	kprintf(" Faulting address: 0x%x\n", context->rip);
	dump_context(context);

	if (vector == 14)
	{

		panic("Page Fault");
	}

	if (vector == 13)
	{
		panic("GPF!");
	}

	pic_acknowledge(vector);
}

static void serial_handler(uint64_t vector, uint64_t code)
{
	// Do nothing
	UNUSED(vector);
	UNUSED(code);
}

void interrupts_install_isr(uint64_t index, void handler(uint64_t, uint64_t))
{
	isr_table[index] = handler;
}

static void pic_init()
{
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
	idt = PHYS_TO_VIRT(&start_idt_64[0]);

	setup_tss_descriptor();

	extern interrupt_handler isr_stub_table[256];

	for (uint64_t i = 0; i < 256; ++i)
	{
		set_idt_entry(i, isr_stub_table[i]);
		interrupts_install_isr(i, &default_handler);
	}

	interrupts_install_isr(36, serial_handler);

	// Initialize the APIC so interrupts from it don't
	// come to the exception interrupt vectors. Currently
	// just disables the APIC.
	apic_init();

	// For now we'll use the old PIC
	pic_init();	
}


