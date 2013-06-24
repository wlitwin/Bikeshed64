.globl main
.globl exit

.globl _start
_start:
	call main

	call exit

_halt:
	hlt
	jmp _halt
