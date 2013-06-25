#include "syscalls.h"
#include "inttypes.h"
#include "types.h"
#include "kernel/scheduler/pcb.h"
#include "kernel/virt_memory/defs.h"
#include "kernel/scheduler/scheduler.h"
#include "kernel/interrupts/defs.h"
#include "kernel/kprintf.h"
#include "kernel/klib.h"


#ifdef BIKESHED_X86_64
#include "arch/x86_64/virt_memory/paging.h"
#include "arch/x86_64/virt_memory/physical.h"
#endif

// The system call lookup table
static void fork(PCB*);
static void exec(PCB*);
static void exit(PCB*);
static void msleep(PCB*);
static void syscall_interrupt(uint64_t vector, uint64_t error);

extern PCB* current_pcb;

static void (*syscall_functions[NUM_SYSCALLS])(PCB*); /*=
{
	fork,  // 0
	exec,  // 1
	exit,  // 2
	msleep // 3
};
*/

//============================================================================
// Fork System Call
//
//============================================================================
void fork(PCB* pcb)
{
	PCB* new_pcb = alloc_pcb();	
	if (new_pcb == NULL)
	{
		// We failed to fork
		pcb->context->rax = FAILURE;
		return;
	}

	memcpy(new_pcb, pcb, sizeof(PCB));

	// Okay we have a new PCB, try to clone the address space
	void* new_page_table = virt_clone_mapping(pcb->page_table);
	new_pcb->page_table = new_page_table;

	uint64_t new_context_addr = 0;
	if (!virt_lookup_phys(new_page_table, (uint64_t)pcb->context, &new_context_addr))
	{
		panic("Fork: failed to lookup context location, very bad!");
	}

	Context* new_context = (Context*)PHYS_TO_VIRT(new_context_addr);

	pcb->context->rax = SUCCESS;	
	*((uint64_t*)pcb->context->rdi) = 0;
	new_context->rax = SUCCESS;

	uint64_t pid_param_location = 0;
	if (!virt_lookup_phys(new_page_table, (uint64_t)new_context->rdi, &pid_param_location))
	{
		panic("Fork: failed to lookup parameter location!");
	}
	Pid* param_pid = (Pid*)PHYS_TO_VIRT(pid_param_location);
	*param_pid = 1;

	kprintf("RBP: 0x%x\n", pcb->context->rbp);

	// Schedule the new pcb, and call dispatch because the timer may not be running
	schedule(new_pcb);
	dispatch();
}

//============================================================================
// Exec System Call
//
//============================================================================
void exec(PCB* pcb)
{

}

//============================================================================
// Exit System Call
//
//============================================================================
void exit(PCB* pcb)
{
	cleanup_pcb(pcb);

	dispatch();
}

//============================================================================
// MSleep System Call
//
//============================================================================
void msleep(PCB* pcb)
{
	
}

void syscalls_init()
{
	syscall_functions[SYSCALL_FORK] = fork;
	syscall_functions[SYSCALL_EXEC] = exec;
	syscall_functions[SYSCALL_EXIT] = exit;
	syscall_functions[SYSCALL_MSLEEP] = msleep;

	interrupts_install_isr(SYSCALL_INT_VEC, syscall_interrupt);
}

void syscall_interrupt(uint64_t vector, uint64_t error)
{
	UNUSED(error);
	
#ifdef BIKESHED_X86_64		
	if (current_pcb == NULL)
	{
		panic("SYSCALL: Current PCB is NULL!");
	}

	// Check the syscall number and dispatch on it
	uint64_t syscall_num = current_pcb->context->r10;
	kprintf("GOT SYSCALL: %u\n", syscall_num);
	kprintf("PARAM1: 0x%x\n", current_pcb->context->rdi);

	if (syscall_num >= NUM_SYSCALLS)
	{
		syscall_functions[SYSCALL_EXIT](current_pcb);
	}
	else
	{
		syscall_functions[syscall_num](current_pcb);
	}

	kprintf("Returned from syscall\n");
#else
#error "System calls are not implemented for this architecture"
#endif

#ifdef BIKESHED_X86_64
	pic_acknowledge(vector);
#endif
}

