#include "scheduler.h"

#include "kernel/panic.h"
#include "kernel/elf/elf.h"
#include "kernel/kprintf.h"
#include "kernel/alloc/alloc.h"
#include "kernel/timer/defs.h"
#include "kernel/virt_memory/defs.h"
#include "kernel/data_structures/block.h"
#include "kernel/data_structures/queue.h"

#ifdef BIKESHED_X86_64
#include "arch/x86_64/virt_memory/physical.h"
#include "arch/x86_64/virt_memory/paging.h"
#endif

static BlockAllocator* ba_pcbs = NULL;
static BlockAllocator* ba_qnodes = NULL;
#define NUM_QUEUES 4
static Queue queues[NUM_QUEUES];
static Queue sleep_queue;

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
	for (uint64_t i = 0; i < NUM_QUEUES; ++i)
	{
		queue_init(&queues[i]);
	}
	queue_init(&sleep_queue);
}

PCB* alloc_pcb()
{
	PCB* pcb = (PCB*) block_alloc(ba_pcbs);
	if (pcb == NULL)
	{
		panic("Failed to allocate first PCB");
	}
	kprintf("Allocating PCB: 0x%x\n", pcb);

	return pcb;
}

void free_pcb(PCB* pcb)
{
	block_free(ba_pcbs, pcb);
}

void create_init_process()
{
	extern uint64_t __KERNEL_END;
	const uint64_t init_location = (const uint64_t)PHYS_TO_VIRT(&__KERNEL_END);
	current_pcb = alloc_pcb();
	kprintf("Current PCB: 0x%x\n", current_pcb);
	kprintf("Kernel End: 0x%x\n", init_location);

	void* new_page_table = virt_clone_mapping(kernel_table);
	current_pcb->page_table = new_page_table;
	current_pcb->state = READY;

	// Switch to the new address space
	virt_switch_page_table(new_page_table);

	uint64_t elf_error = 0;
	if ((elf_error = elf_create_process(current_pcb, 
				(void*)init_location, new_page_table)) != ELF_NO_ERROR)
	{
		kprintf("ELF ERROR: %u\n", elf_error);
		panic("Failed to load init process!");
	}

	// Setup the timer to interrupt init after a little while
	timer_one_shot();

	// The stack has been setup in such a way that calling the ISR restore
	// function will transfer control to the user process.
	__asm__ volatile("jmp isr_restore");
}

void schedule(PCB* pcb)
{
	QueueNode* node = (QueueNode*)block_alloc(ba_qnodes);
	ASSERT(node != NULL);

	node->data = pcb;
	queue_enqueue(&queues[0], node);
}

void cleanup_pcb(PCB* pcb)
{
	virt_switch_page_table(kernel_table);	

	pcb->state = KILLED;
}

// TODO don't go through so many function calls
void timer_interrupt()
{
	dispatch();
}

void dispatch()
{
//	kprintf("Entered dispatch\n");
	// Tell the timer					
	if (!queue_empty(&queues[0]))
	{
		//kprintf("Queue size: %u\n", queue_size(&queues[0]));
//		kprintf("Dispatching\n");
		// Pick the next process
		if (current_pcb->state == KILLED)
		{
			free_pcb(current_pcb);
		}
		else
		{
			schedule(current_pcb);
		}

		QueueNode* node = queue_dequeue(&queues[0]);
		current_pcb = (PCB*)node->data;
		block_free(ba_qnodes, node);
//		kprintf("New PCB: 0x%x\n", current_pcb);
		virt_switch_page_table(current_pcb->page_table);

		timer_one_shot();		
	}
}
