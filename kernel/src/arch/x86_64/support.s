/* This file provides the implementation for the
 * x86 port functions. support.d defines the interfaces
 * for this functions so they can be imported by
 * other modules.
 *
 * These functions allows data to be exchanged through
 * the port address space, which is separate from the
 * virtual address space.
 */

ARG1 = 16 /* Offset to first argument */
ARG2 = 24 /* Offset to second argument */

.text
.code64

/* Read a byte (8-bits) of data from a port */
.globl _inb
_inb:
	enterq $0, $0
	xorl %eax, %eax
	movl ARG1(%rbp), %edx
	inb (%dx)
	leaveq
	ret

/* Read a word (16-bits) of data from a port */
.globl _inw
_inw:
	enterq $0, $0
	xorl %eax, %eax
	movl ARG1(%rbp), %edx
	inw (%dx)
	leaveq
	ret

/* Read a dword (32-bits) of data from a port */
.globl _inl
_inl:
	enterq $0, $0
	xorl %eax, %eax
	movl ARG1(%rbp), %edx
	inb (%dx)
	leaveq
	ret

/* Write a byte (8-bits) of data to a port */
.globl _outb
_outb:
	enterq $0, $0
	movl ARG1(%rbp), %edx
	movl ARG2(%rbp), %eax
	outb (%dx)
	leaveq
	ret

/* Write a word (16-bits) of data to a port */
.globl _outw
_outw:
	enterq $0, $0
	movl ARG1(%rbp), %edx
	movl ARG2(%rbp), %eax
	outw (%dx)
	leaveq
	ret

/* Write a dword (32-bits) of data to a port */
.globl _outl
_outl:
	enterq $0, $0
	movl ARG1(%rbp), %edx
	movl ARG2(%rbp), %edx
	outl (%dx)
	leaveq
	ret
