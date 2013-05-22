ENTRY (pre_kernel)
OUTPUT_ARCH (i386:x86-64)

SECTIONS {

	. = 0xC0200000;
	KERNEL_START = .;

	.text : {
		code = .; _code = .; __code = .;
		*(.text)
		*(.text.*)
	}

	.ctors ALIGN(0x1000) : {
		start_of_ctors = .; 
		*(.ctors) 
		*(.ctors.*) 
		end_of_ctors = .;
	}

	.dtors ALIGN(0x1000) : {
		start_of_dtors = .; 
		*(.dtors) 
		*(.dtors.*) 
		end_of_dtors = .;
	}

	.data ALIGN(0x1000) : {
		data_start = .; __data_start = .;
		data = .; _data = .; __data = .;
		*(.data)
		*(.data.*)
	}

	.tdata ALIGN(0x1000) : {
		tls = .; _tls = .; __tls = .;
		*(.tdata)
		*(.tdata.*)
		*(.gnu.linkonce.td.*)
	}

	.rodata ALIGN(0x1000) : {
		*(.rodata)
		*(.rodata.*)
		*(.rel.rodata)
		*(.rel.rodata.*)
	}

	.tbss ALIGN(0x1000) : {
		tbss = .; _tbss = .; __tbss = .;
		*(.tbss) /* Thread local storage stuff */
		*(.tbss.*)
		*(.gnu.linkonce.tb.*)
		*(.tcommon)
	}

	.bss : ALIGN(0x1000) {
		sbss = .;
		bss = .; _bss = .; __bss = .;
		*(COMMON)
		*(.bss)
		*(.bss.*)
	/*	*(.tbss) 
		*(.tbss.*)
		*(.tcommon)
		*/
		ebss = .;
	}
	end = .; _end = .; __end = .;
	KERNEL_END = ALIGN(0x4);

	/DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
	}
}
