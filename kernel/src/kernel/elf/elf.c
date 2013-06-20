#include "elf.h"

#include "kernel/klib.h"
#include "kernel/virt_memory/defs.h"

ELF_Error elf_create_process(void* elf_file, void* page_table)
{
	ELF64_Ehdr* elf_hdr = (ELF64_Ehdr*)elf_file;

	// Check some constants
	if (elf_hdr->e_ident[0] != El_MAG0 ||
		elf_hdr->e_ident[1] != El_MAG1 ||
		elf_hdr->e_ident[2] != El_MAG2 ||
		elf_hdr->e_ident[3] != El_MAG3)
	{
		return ELF_ERROR_BAD_MAGIC;	
	}

	// Check the ELF class, must be ELF64
	if (elf_hdr->e_ident[ELF_CLASS_OFF] != ELF_CLASS_64)
	{
		return ELF_ERROR_BAD_CLASS;
	}

	// Check the data encoding
	if (elf_hdr->e_ident[ELF_DATA_OFF] != ELF_DATA2)
	{
		return ELF_ERROR_BAD_ENDIAN;
	}

	// Operating system ABI
	if (elf_hdr->e_ident[ELF_OSABI_OFF] != ELF_ABI)
	{
		return ELF_ERROR_BAD_ABI;
	}

	// Check the file type
	if (elf_hdr->e_type != ET_EXEC)// &&
		//elf_hdr->e_type != ET_DYN)
	{
		return ELF_ERROR_BAD_TYPE;
	}

	virt_reset_table(page_table);



	// Otherwise we're good
	return ELF_NO_ERROR;
}
