/* Contains the interrupt handler stubs. This makes all interrupts
 * conform to the same signature.
 */

.text
.code64
.align 16

/* The following two macros were adapted from SOLARIS code:
 * https://hg.java.net/hg/solaris~on-src/file/tip/usr/src/uts/intel/amd64/sys/privregs.h
 */

/* Saves all registers onto the stack
 */
.macro SAVE_ALL_REGS
	movq 	%r15,   0(%rsp)
	movq	%r14,   8(%rsp)
	movq	%r13,  16(%rsp)
	movq	%r12,  24(%rsp)
	movq	%r11,  32(%rsp)
	movq	%r10,  40(%rsp)
	movq	%r9,   48(%rsp)
	movq	%r8,   56(%rsp)
	movq	%rbp,  64(%rsp)
	movq	%rax,  72(%rsp)
	movq	%rbx,  80(%rsp)
	movq	%rcx,  88(%rsp)
	movq	%rdx,  96(%rsp)
	movq	%rsi, 104(%rsp)
	movq	%rdi, 112(%rsp)
	/*xorl	%ecx, %ecx
	movw	%gs, %cx
	movq	%rcx, 118(%rsp)
	movw	%fs, %cx
	movq	%rcx, 126(%rsp)
	movw	%ds, %cx
	movq	%rcx, 134(%rsp)
	*/
.endm

/* Restores all registers from the stack
 */
.macro RESTORE_ALL_REGS

.endm
