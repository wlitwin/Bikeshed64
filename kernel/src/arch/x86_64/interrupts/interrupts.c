#include "interrupts.h"

#include "tss.h"
#include "apic.h"
#include "safety.h"
#include "imports.h"

#include "kernel/klib.h"

#include "arch/x86_64/panic.h"
#include "arch/x86_64/kprintf.h"
#include "arch/x86_64/virt_memory/physical.h"
#include "arch/x86_64/virt_memory/paging.h"
#include "kernel/scheduler/scheduler.h"
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

void (*isr_table[256])(uint64_t vector, uint64_t code);

typedef void (*interrupt_handler)(void);

static void set_idt_entry(uint64_t index, interrupt_handler fn_ih)
{
	if (index > 255)
	{
		panic("Bad interrupt handler!");
	}

	const uint64_t fn_loc = (uint64_t)fn_ih;

	idt[index].offset_L = fn_loc & 0xFFFF;
	idt[index].segment_selector = CODE_SEG_64;
	idt[index].ist = 0;
	// We didn't really define 0x80 anywhere, but it's the syscall interrupt
	if (index != 0x80)
	{
		// Don't allow user land processes to use this interrupt
		idt[index].flags = 0x8E;
	}
	else
	{
		// Allow user processes to use this interrupt
		idt[index].flags = 0xEE;
	}
	idt[index].offset_H = (fn_loc >> 16) & 0xFFFF;
	idt[index].offset_HH = (fn_loc >> 32) & 0xFFFFFFFF;
	idt[index].reserved = 0;
}

void dump_context(Context* context)
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
	kprintf("cs : 0x%x\n", context->cs);
	kprintf("rfl: 0x%x\n", context->rflags);
	kprintf("rsp: 0x%x\n", context->rsp);
	kprintf("ss : 0x%x\n", context->ss);
	kprintf("rip: 0x%x\n", context->rip);
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

static void spurious_handler(uint64_t vector, uint64_t code)
{
	UNUSED(vector);
	UNUSED(code);

	kprintf("SPURIOUS\n");
	__asm__ volatile("hlt");
	pic_acknowledge(vector);
}

void interrupts_install_isr(uint64_t index, void handler(uint64_t, uint64_t))
{
	isr_table[index] = handler;
}

// Used as a dummy pcb during kernel initialization
static PCB init_pcb;

void interrupts_init()
{
	// Create a dummy process in case we receive an exception during the rest of kernel
	// initialization. The interrupts won't work unless we have something in current_pcb
	memclr(&init_pcb, sizeof(PCB));
	init_pcb.page_table = kernel_table;	
	current_pcb = &init_pcb;

	idt = PHYS_TO_VIRT(&start_idt_64[0]);

	setup_tss_descriptor();

	extern interrupt_handler isr_stub_table[256];

	for (uint64_t i = 0; i < 256; ++i)
	{
		set_idt_entry(i, isr_stub_table[i]);
		interrupts_install_isr(i, &default_handler);
	}

	interrupts_install_isr(36, serial_handler);
	interrupts_install_isr(39, spurious_handler);

	// Initialize the APIC
	apic_init();
}

