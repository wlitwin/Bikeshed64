ENTRY (bootloader_entry)
OUTPUT_ARCH (i386)

SECTIONS {
	. = 0x0;

	.text : {
		*(.text)
	}

	.data : {
		*(.data)
	}
}
