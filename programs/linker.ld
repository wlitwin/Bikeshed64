ENTRY (_start)
OUTPUT_ARCH (i386:x86-64)
OUTPUT_FORMAT (elf64-x86-64)

SECTIONS {

	/* All code/data starts at 0x8000000 */
	virt = 0x8000000;
	. = virt;

	.text ALIGN(0x200000) : AT(0x0) {
		code = .; _code = .; __code = .;
		*(.text)
		*(.text.*)
		code_end = .;
	}

	.data ALIGN(0x200000) : AT(code_end - virt) {
		data_start = .; __data_start = .;
		data = .; _data = .; __data = .;
		*(.data)
		*(.data.*)
		start_of_ctors = .; *(.ctors) *(.ctors.*) end_of_ctors = .;
		start_of_dtors = .; *(.dtors) *(.dtors.*) end_of_dtors = .;
		data_end = .;
	}

	.rodata ALIGN(0x200000) : AT(data_end - virt) {
		*(.rodata)
		*(.rodata.*)
		*(.rel.rodata)
		*(.rel.rodata.*)
		rodata_end = .;
	}

	.bss ALIGN(0x200000) : AT(rodata_end - virt) {
		sbss = .;
		bss = .; _bss = .; __bss = .;
		*(COMMON)
		*(.bss)
		*(.bss.*)
		ebss = .;
	}
	end = .; _end = .; __end = .;

	/DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
	}
}
