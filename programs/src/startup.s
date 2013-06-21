.globl main

.globl
_start:
	call main

_halt:
	hlt
	jmp _halt
