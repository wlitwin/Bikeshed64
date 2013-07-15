.globl main
.globl exit

.globl _start
_start:

	# Clear the BSS section
	movabsq $sbss, %rdi
	movabsq $ebss, %rax
clear_bss_loop:
	movq $0, (%rdi)
	addq $8, %rdi
	cmpq %rax, %rdi
	jb clear_bss_loop

	call main

	call exit

_halt:
	jmp _halt
