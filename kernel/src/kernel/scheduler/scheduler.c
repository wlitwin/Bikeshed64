#include "scheduler.h"

#include "kernel/panic.h"
#include "kernel/elf/elf.h"
#include "kernel/kprintf.h"
#include "kernel/alloc/alloc.h"
#include "kernel/virt_memory/defs.h"
#include "kernel/data_structures/block.h"
#include "kernel/data_structures/queue.h"

#ifdef BIKESHED_X86_64
#include "arch/x86_64/virt_memory/physical.h"
#include "arch/x86_64/virt_memory/paging.h"
#endif

static BlockAllocator* ba_pcbs = NULL;
static BlockAllocator* ba_qnodes = NULL;
static Queue queues[4];

#define MAX_PCBS 1024
#define MAX_QUEUE_NODES 2048

PCB* current_pcb = NULL;

void scheduler_init()
{
	// Allocate space for the PCBs
	const uint64_t pcb_size_needed = sizeof(PCB)*MAX_PCBS + sizeof(BlockAllocator);
	const void* ba_address = water_mark_alloc(&kernel_WaterMark, pcb_size_needed);
	ba_pcbs = block_init(ba_address, pcb_size_needed, sizeof(PCB));

	// Allocate space for the QueueNodes
	const uint64_t queue_node_size_needed = sizeof(QueueNode)*MAX_QUEUE_NODES + sizeof(QueueNode);
	const void* qn_address = water_mark_alloc(&kernel_WaterMark, queue_node_size_needed);
	ba_qnodes = block_init(qn_address, queue_node_size_needed, sizeof(QueueNode));

	// Initialize all of the queues
}

void create_init_process()
{
	extern uint64_t __KERNEL_END;
	const uint64_t init_location = (const uint64_t)PHYS_TO_VIRT(&__KERNEL_END);
	current_pcb = (PCB*)block_alloc(ba_pcbs);
	if (current_pcb == NULL)
	{
		panic("Failed to allocate first PCB");
	}
	kprintf("Current PCB: 0x%x\n", current_pcb);
	kprintf("Kernel End: 0x%x\n", init_location);

	void* new_page_table = virt_clone_mapping(kernel_table);
	current_pcb->page_table = new_page_table;
	// Switch to the new address space
	write_cr3(new_page_table);


	uint64_t elf_error = 0;
	if ((elf_error = elf_create_process(current_pcb, 
				(void*)init_location, new_page_table)) != ELF_NO_ERROR)
	{
		kprintf("ELF ERROR: %u\n", elf_error);
		panic("Failed to load init process!");
	}

	__asm__ volatile("jmp isr_restore");
}

void schedule(PCB* pcb)
{

}

void dispatch()
{

}
