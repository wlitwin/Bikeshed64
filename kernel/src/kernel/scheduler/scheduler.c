#include "scheduler.h"

#include "kernel/klib.h"
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

#ifndef DEBUG_SCHEDULER
#define kprintf(...)
#endif

static BlockAllocator* ba_pcbs = NULL;
static BlockAllocator* ba_qnodes = NULL;
#define NUM_QUEUES 4
static Queue queues[NUM_QUEUES];
static Queue sleep_queue;

#define MAX_PCBS 1024
#define MAX_QUEUE_NODES 2048

PCB* current_pcb = NULL;

// Used for tracking the next sleep wakening
static time_t prev_ticks = 0;
static time_t quantum_left = 0;

// Cache some values from the timer
static time_t ten_ms;
static time_t one_ms;

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

	pcb->state = READY;
	pcb->priority = NORMAL;

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
	one_ms = timer_one_ms();
	ten_ms = one_ms * 10;
	kprintf("1MS: %u - 10MS: %u\n", one_ms, ten_ms);
	prev_ticks = quantum_left = 10;
	timer_set_delay(quantum_left*one_ms);
	timer_start();

	// The stack has been setup in such a way that calling the ISR restore
	// function will transfer control to the user process.
	__asm__ volatile("jmp isr_restore");
}

void cleanup_pcb(PCB* pcb)
{
	virt_switch_page_table(kernel_table);	

	virt_cleanup_table(pcb->page_table);

	pcb->state = KILLED;
}

int8_t sleep_insert(const void* d1, const void* d2)
{
	// pcb1 == the node we want to insert
	PCB* pcb1 = (PCB*)d1;
	PCB* pcb2 = (PCB*)d2;

	// return 1 if pcb1 > pcb2, 0 if pcb1 == pcb2
	// and < 0 if pcb1 < pcb2
	if (pcb1->sleep_time <= pcb2->sleep_time)
	{
		pcb2->sleep_time -= pcb1->sleep_time;
		return -1;
	}
	else
	{
		pcb1->sleep_time -= pcb2->sleep_time;
		return 1;
	}
}

uint8_t schedule(PCB* pcb)
{
	switch (pcb->state)
	{
		case READY:
			{
				QueueNode* node = (QueueNode*)block_alloc(ba_qnodes);
				ASSERT(node != NULL);
				ASSERT(pcb->priority < NUM_QUEUES);

				node->data = pcb;
				queue_enqueue(&queues[pcb->priority], node);
			}
			break;
		case SLEEPING:
			{
				QueueNode* node = (QueueNode*)block_alloc(ba_qnodes);
				ASSERT(node != NULL);
				node->data = pcb;

				queue_enqueue_prio(&sleep_queue, node, sleep_insert);
			}
			break;
		default:
			{
				panic("Scheduler: Unhandled PCB state");
			}
			break;
	}

	return 1;
}

void sleep_pcb(PCB* pcb, time_t time)
{
	if (time != 0)
	{
		kprintf("Sleeping for: %u\n", time);
		// Add to the sleep queue
		pcb->state = SLEEPING;
		pcb->sleep_time = time;
		kprintf("(Time): %u\n", pcb->sleep_time);
	}

	dispatch();
}

// TODO don't go through so many function calls
void timer_interrupt()
{
	dispatch();
}

uint32_t get_next_sleep(const uint32_t tick_span)
{
	if (queue_empty(&sleep_queue))
	{
		return 0;
	}

	QueueNode* peek = queue_peek(&sleep_queue);
	PCB* pcb = (PCB*)peek->data;
	if (pcb->sleep_time <= tick_span)
	{
		// Wake this PCB up
		pcb->state = READY;
		pcb->sleep_time = 0;
		schedule(pcb);
		block_free(ba_qnodes, queue_dequeue(&sleep_queue));

		// Check the next guy
		if (!queue_empty(&sleep_queue))
		{
			peek = queue_peek(&sleep_queue);
			pcb = (PCB*)peek->data;

			return pcb->sleep_time;
		}

		return 0;
	}
	else
	{
		pcb->sleep_time -= tick_span;
		return pcb->sleep_time;
	}
}

void dispatch()
{
	const uint32_t elapsed = (timer_get_elapsed() / one_ms);
	const uint32_t tick_span = elapsed;
	kprintf("ELAPSED: %u - PREV: %u\n", elapsed, prev_ticks);
	kprintf("Tick Span: %u\n", tick_span);

	// Adjust the quantum appropriately
	if (tick_span > quantum_left) { quantum_left = 0; }
	else { quantum_left -= tick_span; }
	kprintf("Quantum: %u\n", quantum_left);

	/*if (queue_empty(&sleep_queue) && queue_empty(&queues[0]))
	{
		prev_ticks = quantum_left;
		return;
	}
	*/

	// TODO do an initial subtraction from head of sleep queue?
	if (current_pcb->state == KILLED)
	{
		cleanup_pcb(current_pcb);
		current_pcb = NULL;
		quantum_left = 10;
	}
	else if (current_pcb->state == SLEEPING || quantum_left == 0)
	{
		kprintf("PCB is going to sleep\n");
		schedule(current_pcb);
		current_pcb = NULL;
		quantum_left = 10;
	}

	prev_ticks = quantum_left;

	// Check the next wakeup
	const uint32_t next_wakeup = get_next_sleep(tick_span);
	if (next_wakeup > 0 && next_wakeup < quantum_left)
	{
		prev_ticks = next_wakeup;
	}

	if (current_pcb != NULL)
	{
		kprintf("Not done! 0x%x\n", current_pcb);
		ASSERT(quantum_left != 0);
		timer_set_delay(prev_ticks*one_ms);
		timer_start();
		return;
	}

	kprintf("PREV_TICKS: %u\n", prev_ticks);
	ASSERT(prev_ticks > 0);

	// Pick the next person to run	
	for (uint64_t i = 0; i < NUM_QUEUES; ++i)
	{
		while (!queue_empty(&queues[i]))
		{
			QueueNode* node = queue_dequeue(&queues[i]);
			ASSERT(node != NULL);
			PCB* next = (PCB*)node->data;
			block_free(ba_qnodes, node);

			switch (next->state)
			{
				case KILLED:
					free_pcb(next);
					continue;
				case READY:
					{
						kprintf("Next: 0x%x\n", next);
						current_pcb = next;
						virt_switch_page_table(current_pcb->page_table);
						timer_set_delay(prev_ticks*one_ms);
						timer_start();
					}
					return;
				default:
					panic("Scheduler: Unhandled case!");
					break;
			}
		}
	}

	panic("Dispatch: Nothing left to run!");
}
