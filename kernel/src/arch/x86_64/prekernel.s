
PAE_BIT = 0x00020 # CR4 Physical Address Extension
LME_BIT = 0x0100 # EFER Long Mode Enable

VIDEO_RAM = 0xB8000

KERNEL_LOAD_LOCATION = 0x200000
KERNEL_BASE_LOCATION = 0xFFFF800000000000
KERNEL_STACK_LOCATION = KERNEL_BASE_LOCATION + (KERNEL_LOAD_LOCATION - 0x4)

/********************************************************
 * GDT Information
 *******************************************************/

/* Need 64-bit GDT entries 
 * Align them to a dword boundary according to AMD 
 * Systems Programming Guide
 */
.section data_prekernel,"wa",@progbits
.align 32
.globl start_gdt_64
start_gdt_64:

null_selector:
	.quad 0
	.quad 0

code_seg_64:
	.hword 0
	.hword 0
	.byte 0
	.byte 0b10011000
	.byte 0b00100000
	.byte 0
	# Align to 16-bytes
	.quad 0

data_seg_64:
	.hword 0xFFFF
	.hword 0
	.byte 0
	.byte 0b10000000
	.byte 0b00001111
	.byte 0
	# Align to 16 bytes
	.quad 0

.globl tss_seg_64
tss_seg_64:
	.fill 2, 8, 0 # 16-byte descriptor in 64-bit mode

end_gdt_64:
gdt_64_len = end_gdt_64 - start_gdt_64

.align 16
gdt_64:
	.word gdt_64_len
	.quad start_gdt_64

/********************************************************
 * End GDT Information
 *******************************************************/

/********************************************************
 * IDT Information
 *******************************************************/
.align 16
idt_64:
	.word idt_64_len
	.quad start_idt_64
	
.align 4096
.globl start_idt_64
start_idt_64:
	.fill 512, 8, 0	# .fill only supports a size of 8, so we double the number
end_idt_64:
idt_64_len = end_idt_64 - start_idt_64

/********************************************************
 * End IDT Information
 *******************************************************/

string_cpuid_error:
.asciz "CPUID is not supported, HALTING"

string_not_64bit:
.asciz "This is not a 64-bit processor, HALTING"

string_ext_func_not_supported:
.asciz "Processor does not support enough extended functions for CPUID, HALTING"

print_offset:
.word 0

/********************************************************
 * Paging Information
 *******************************************************/

PWT_BIT = 0x4 # Page-level write-through
PCD_BIT = 0x8 # Page-level cache disable
PRESENT = 0x1
KERNEL_PML4_VALUE = 0b11

.align 4096
.globl kernel_PML4
kernel_PML4:
	.quad (kernel_PDPTE + KERNEL_PML4_VALUE)
	.fill 255, 8, 0
	.quad (kernel_PDPTE + KERNEL_PML4_VALUE)
	.fill 255, 8, 0

.align 4096
.globl kernel_PDPTE
kernel_PDPTE:
	#.quad 0b110000011
	.quad (kernel_PDT + 0b11)
	.fill 511, 8, 0

/* This macro craziness to so we can identiy map the first 1GiB
 * of address space. This will make it easier for the physical
 * memory manager to set itself up, as paging is already enabled,
 * yet it has to move stuff around in non-virtual memory. The reason
 * the macro had to be written out six times is because GAS has a
 * problem with macro recursion greater than a depth of 100. The
 * command to increase the depth was not immediately obvious so
 * this was done as a work around.
 */
.align 4096
.globl kernel_PDT
kernel_PDT:
	START_VAL = 0
	.rept 512
	.quad START_VAL + 0b010000011
	START_VAL = START_VAL + 0x200000
	.endr

/********************************************************
 * End Paging Information
 *******************************************************/

/********************************************************
 * Processor Address Size Information
 *******************************************************/
.globl processor_phys_bits
processor_phys_bits:
	.byte 0
.globl processor_virt_bits
processor_virt_bits:
	.byte 0

/********************************************************
 * End Processor Address Size Information
 *******************************************************/

.section text_prekernel,"xa",@progbits
.code32
.align 4096
.globl pre_kernel
pre_kernel:
	/* Before proceeding we need to learn some information
	 * about the processor we're using. First it needs to
	 * support the CPUID instruction because that will tell
	 * us about the features this CPU supports.
	 *
	 * Taken from:
	 * 	http://wiki.osdev.org/CPUID
	 */
	pushfl
	popl %eax
	movl %eax, %ecx
	xorl $0x200000, %eax # Flip ID flag of EFLAGS
	pushl %eax 
	popfl 			   # Try to modify EFLAGS
	pushfl
	popl %eax		   # Check if it was modified
	xorl %ecx, %eax     # Mask bit 21
	shrl $21, %eax	   # Move bit 21 -> bit 0
	andl $1, %eax	   # Test the bit
	pushl %ecx
	popfl			   # Restore EFLAGS

	/* EAX contains 1 if CPUID is supported, 0 otherwise */
	test %eax, %eax
	jnz cpuid_supported

	/* CPUID is not supported, we need to give some kind of error */
	mov $string_cpuid_error, %eax
	jmp print_error_and_halt

cpuid_supported:

	/* Figure out the max CPUID command, must be at least 0x80000008 */
	movl $0x80000000, %eax
	cpuid

	/* We need to support at least the 0x80000008 extended functions
	 * for CPUID, this tells us the number of bits implemented for
	 * physical and virtual addresses
	 */
	cmpl $0x80000008, %eax
	jge extended_functions_supported

	mov $string_ext_func_not_supported, %eax
	jmp print_error_and_halt

extended_functions_supported:

	/* Before we continue we need to know if the CPU supports
	 * certain things, like 64-bit mode and 1GiB pages.
	 */
	movl $0x80000001, %eax
	cpuid

	/* Check if this processor supports 64-bit mode */
	testl $0x20000000, %edx
	jnz ia32e_supported

	/* 64-bit mode is not supported, give an error */
	movl $string_not_64bit, %eax
	jmp print_error_and_halt

ia32e_supported:

	/* Figure out how many address bits are supported by this processor
	 * for virtual and physical addresses
	 */
	movl $0x80000008, %eax
	cpuid

	/* EAX [7:0]  - Physical address size
	 * EAX [15:8] - Virtual address size
	 */
	movb %al, processor_phys_bits
	shr $8, %eax
	movb %al, processor_virt_bits

	/* IA32_EFER is cleared on reset 
	 * The recommended sequence of initialization is:
	 *  - Clear the paging bit (already done)
	 *  - Enable PAE (in CR4)
	 *  - Load CR3 with base address of PML4
	 *  - Set IA32_EFER.LME to 1
	 *  - Enable paging (CR0.PG = 1)
	 *
	 * Some notes:
	 *  64-bit page tables must be located in the first 4GBytes
	 *  of RAM prior to enabling IA-32e mode. This is because
	 *  in protected mode CR3 is only 32-bits wide.
	 */

	/* Enable PAE */
	movl %cr4, %eax
	orl  $PAE_BIT, %eax
	movl %eax, %cr4

	/* Load CR3 with PML4 */
	movl $kernel_PML4, %eax
	movl %eax, %cr3

	/* Enable EFER.LME */
	mov  $0xC0000080, %ecx
	rdmsr
	or   $LME_BIT, %eax
	wrmsr

	/* Enable paging */
	movl %cr0, %eax
	orl  $0x80000000, %eax
	movl %eax, %cr0

	/* Load the new GDT */
	lgdt (gdt_64)	

	/* Load the new IDT */
	lidt (idt_64)

	/* Jump + Enable Long Mode! */
	jmp $0x10, $longmode_code

/* Prints a string
 *
 * Params:
 *   EAX - The address of the NULL terminated string to print
 */
print_string:
	pushl %ebx
	pushl %ecx
	pushl %edx

	movl $VIDEO_RAM, %ecx		# Get the video location
	movw print_offset, %bx		# Get the current offset	

ps_loop:
	mov (%eax), %dl		# Grab a character

	test %dl, %dl		# Test if the character is NULL
	jz ps_done

	inc %eax			# Advance to the next character

	movb %dl, 0(%ecx, %ebx, 2) 	# Put the character on the screen
	movb $0x9, 1(%ecx, %ebx, 2)	# Blue text, Black background
	inc %ebx					# Advance the 'cursor'

	# Make sure EBX stays in range
	cmpl $(80*25), %ebx
	jl ps_in_range

	movl $0, %ebx				# Reset to 0
	
ps_in_range:
	jmp ps_loop

ps_done:
	movw %bx, print_offset		# Save the current offset

	popl %edx
	popl %ecx
	popl %ebx
	ret

/* Prints an error message to video ram and halts.
 * For simplicity it assumes the error message is less
 * than 80 characters (so it fits on one line).
 *
 * Params:
 *   EAX - String location
 */
print_error_and_halt:
	call print_string

peat_done:
	hlt
	jmp peat_done

/* Start of the 64-bit code */
.code64
.globl longmode_code
longmode_code:
	/* Setup kernel stack */
	movq $KERNEL_STACK_LOCATION, %rsp
	movq $KERNEL_STACK_LOCATION, %rbp

	/* Clear the BSS section */
	movabsq $sbss, %rdi
	movabsq $ebss, %rax
clear_bss_loop:
	movq $0, (%rdi)
	addq $8, %rdi
	cmpq %rax, %rdi
	jb clear_bss_loop

	movabsq $kmain, %rax
	/* Give control to the kernel */
	call *%rax

	/* Kernel returned, wait forever */
	cli
kernel_returned:
	hlt
	jmp kernel_returned
