/* Contains the interrupt handler stubs. This makes all interrupts
 * conform to the same signature.
 */

#include "arch/x86_64/virt_memory/defines.h"

.text
.code64
.align 16

/* The following two macros were adapted from SOLARIS code:
 * https://hg.java.net/hg/solaris~on-src/file/tip/usr/src/uts/intel/amd64/sys/privregs.h
 */

/* Can't find any documentation on what is allowed in GAS .if statments
 */
.macro ISR_STUB int_num=0
	isr_\int_num:
	cli
	.if \int_num == 8
	.elseif \int_num == 10
	.elseif \int_num == 11
	.elseif \int_num == 12
	.elseif \int_num == 13
	.elseif \int_num == 14
	.elseif \int_num == 17
	.else
	pushq	$0
	.endif

	pushq 	$\int_num

	jmp isr_save
.endm

.globl current_pcb
isr_save:
	/* Save the registers */
	pushq	%r15
	pushq	%r14
	pushq	%r13
	pushq	%r12
	pushq	%r11
	pushq	%r10
	pushq	%r9
	pushq	%r8
	pushq	%rbp
	pushq	%rax
	pushq	%rbx
	pushq	%rcx
	pushq	%rdx
	pushq	%rsi
	pushq	%rdi
/*	xorl	%ecx, %ecx
	movw	%gs, %cx
	movq	%rcx, 118(%rsp)
	movw	%fs, %cx
	movq	%rcx, 126(%rsp)
	movw	%ds, %cx
	movq	%rcx, 134(%rsp)
	*/

	movq	%rsp, %rbx
	movabs	current_pcb, %rax
	movq	%rbx, (%rax)

	/* Grab the error code off the stack */
	movq	120(%rsp), %rax
	movq	128(%rsp), %rbx

	/* Change over to the kernel's stack */
	movabs	$KERNEL_STACK_LOCATION - 0x8000, %rsp

	/* Pass them as arguments to the handler
	 * x86_64 calling convention on linux uses
	 * some registers for the arguments
	 */
	movq	%rax, %rdi
	movq	%rbx, %rsi

	.globl isr_table
	movabsq  $isr_table, %rbx
	movq	0(%rbx, %rax, 8), %rbx
	call	*%rbx

	jmp isr_restore

.globl isr_restore
isr_restore:
	movabs	current_pcb, %rax		
	movq	(%rax), %rsp

	/* Restore all the registers */
	popq	%rdi
	popq	%rsi
	popq	%rdx
	popq	%rcx
	popq	%rbx
	popq	%rax
	popq	%rbp
	popq	%r8
	popq	%r9
	popq	%r10
	popq	%r11
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15

	/* Pop the vector and error code pushed by the ISR stub */
	addq	$16, %rsp

/*
	movabs current_pcb, %rax
	cmpq $0xFFFFFFFFFFDF8020, %rax
	jne ret_it
	hlt
ret_it:
*/

	iretq

.data

/* GAS macros aren't very friendly. Couldn't find any documentation that would have made
 * this easier to do. Also the very shallow limit on nested macros is annoying
 */
.irp isr_num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48, 49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71, 72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94, 95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113, 114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131, 132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149, 150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167, 168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185, 186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203, 204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221, 222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239, 240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
	ISR_STUB \isr_num
.endr

.align 8
.globl isr_stub_table
isr_stub_table:
.irp isr_num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48, 49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71, 72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94, 95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113, 114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131, 132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149, 150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167, 168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185, 186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203, 204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221, 222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239, 240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
.quad isr_\isr_num
.endr
