#include "syscalls.h"
#include "inttypes.h"
#include "types.h"
#include "kernel/timer/defs.h"
#include "kernel/scheduler/pcb.h"
#include "kernel/virt_memory/defs.h"
#include "kernel/scheduler/scheduler.h"
#include "kernel/keyboard/defs.h"
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
static void set_priority(PCB*);
static void key_avail(PCB*);
static void get_key(PCB*);
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
	*((Pid*)pcb->context->rdi) = 0;
	new_context->rax = SUCCESS;

	uint64_t pid_param_location = 0;
	if (!virt_lookup_phys(new_page_table, (uint64_t)new_context->rdi, &pid_param_location))
	{
		panic("Fork: failed to lookup parameter location!");
	}
	Pid* param_pid = (Pid*)PHYS_TO_VIRT(pid_param_location);
	*param_pid = 1;

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
	UNUSED(pcb);
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
	const time_t sleep_time = pcb->context->rdi;

	// NOTE: When you return from this function, the address space may have
	//       changed and the passed in pcb CANNOT be used unless something
	//       like the fork() system call is done, where the physical address
	//       is looked up.
	sleep_pcb(pcb, sleep_time);
}

//============================================================================
// Set the priority
//
//============================================================================
void set_priority(PCB* pcb)
{
	const uint64_t priority = pcb->context->rdi;
	ASSERT(priority < 4);
	pcb->priority = (Priority)priority;
}

void key_avail(PCB* pcb)
{
	pcb->context->rax = keyboard_char_available();	
	//kprintf("Key Avail: 0x%x\n", pcb->context->rax);
}

void get_key(PCB* pcb)
{
	pcb->context->rax = keyboard_get_char();
	//kprintf("Get Key: 0x%x\n", pcb->context->rax);
}

void syscalls_init()
{
	syscall_functions[SYSCALL_FORK] = fork;
	syscall_functions[SYSCALL_EXEC] = exec;
	syscall_functions[SYSCALL_EXIT] = exit;
	syscall_functions[SYSCALL_MSLEEP] = msleep;
	syscall_functions[SYSCALL_SET_PRIO] = set_priority;
	syscall_functions[SYSCALL_KEY_AVAIL] = key_avail;
	syscall_functions[SYSCALL_GET_KEY] = get_key;

	interrupts_install_isr(SYSCALL_INT_VEC, syscall_interrupt);
}

void syscall_interrupt(uint64_t vector, uint64_t error)
{
	UNUSED(error);
	// Stop the current processes quantum
	timer_stop();
	
#ifdef BIKESHED_X86_64		
	if (current_pcb == NULL)
	{
		panic("SYSCALL: Current PCB is NULL!");
	}

	// Check the syscall number and dispatch on it
	uint64_t syscall_num = current_pcb->context->r10;
//	kprintf("GOT SYSCALL: %u\n", syscall_num);
//	kprintf("PARAM1: 0x%x\n", current_pcb->context->rdi);

	if (syscall_num >= NUM_SYSCALLS)
	{
		kprintf("BAD SYSCALL\n");
		syscall_functions[SYSCALL_EXIT](current_pcb);
	}
	else
	{
		syscall_functions[syscall_num](current_pcb);
	}

//	kprintf("Returned from syscall\n");
#else
#error "System calls are not implemented for this architecture"
#endif

#ifdef BIKESHED_X86_64
	pic_acknowledge(vector);
#endif
}

