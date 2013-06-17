#include "scheduler.h"

#include "kernel/panic.h"
#include "kernel/kprintf.h"
#include "kernel/alloc/alloc.h"
#include "kernel/data_structures/block.h"
#include "kernel/data_structures/queue.h"

static BlockAllocator* ba_pcbs;
static BlockAllocator* ba_qnodes;
static Queue queues[4];

#define MAX_PCBS 1024
#define MAX_QUEUE_NODES 2048

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
}

void schedule(PCB* pcb)
{

}

void dispatch()
{

}
