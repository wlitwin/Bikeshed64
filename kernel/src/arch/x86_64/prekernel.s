
PAE_BIT = 0x00020 # CR4 Physical Address Extension
LME_BIT = 0x0100 # EFER Long Mode Enable

KERNEL_LOAD_LOCATION = 0x200000
KERNEL_BASE_LOCATION = 0x000000
KERNEL_STACK_LOCATION = KERNEL_BASE_LOCATION + (KERNEL_LOAD_LOCATION - 0x4)

/* Need 64-bit GDT entries 
 * Align them to a dword boundary according to AMD 
 * Systems Programming Guide
 */
.data
.align 32
start_gdt_64:

null_selector:
	.quad 0

code_seg_64_compat:
	
data_seg_64_compat:

stack_seg_64_compat:
	
code_seg_64:
	.word 0xFFFF
	.word 0
	.byte 0
	.byte 0b10011000
	.byte 0b00100000
	.byte 0

data_seg_64:
	.word 0xFFFF
	.word 0
	.byte 0
	.byte 0b10000000
	.byte 0b00001111
	.byte 0

end_gdt_64:
gdt_64_len = end_gdt_64 - start_gdt_64

gdt_64:
	.word gdt_64_len
	.quad start_gdt_64

idt_64:
		
PWT_BIT = 0x4 # Page-level write-through
PCD_BIT = 0x8 # Page-level cache disable
PRESENT = 0x1
KERNEL_PML4_VALUE = 0b11

.align 4096
kernel_PML4:
	.quad (kernel_PDPTE + KERNEL_PML4_VALUE)
	.fill 511, 8, 0

.align 4096
kernel_PDPTE:
	.quad 0b110000011
	.fill 511, 8, 0

.text
.code32
.align 4096
.globl pre_kernel
pre_kernel:
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


	/* Jump + Enable Long Mode! */
	jmp $8, $longmode_code

.code64
longmode_code:
	/* Setup kernel stack */
	movq $KERNEL_STACK_LOCATION, %rsp
	movq $KERNEL_STACK_LOCATION, %rbp

	/* Clear the BSS section */
	movq $sbss, %rdi
clear_bss_loop:
	movq $0, (%rdi)
	addq $8, %rdi
	cmpq $ebss, %rdi
	jb clear_bss_loop

	/* Give control to the kernel */
	call kmain

	/* Kernel returned, wait forever */
	cli
kernel_returned:
	hlt
	jmp kernel_returned
	
