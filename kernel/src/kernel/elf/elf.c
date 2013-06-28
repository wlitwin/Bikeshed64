#include "elf.h"

#include "kernel/klib.h"
#include "kernel/kprintf.h"
#include "kernel/virt_memory/defs.h"

#ifdef BIKESHED_X86_64
#include "arch/x86_64/virt_memory/physical.h"
#include "arch/x86_64/interrupts/imports.h"
#endif

ELF_Error elf_create_process(PCB* pcb, void* elf_file, void* page_table)
{
	const uint64_t elf_address = (uint64_t)elf_file;
	const ELF64_Ehdr* elf_hdr = (const ELF64_Ehdr*)elf_file;

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

	// Check the architecture type
	if (elf_hdr->e_machine != ELF_CUR_MACHINE)
	{
		return ELF_ERROR_BAD_MACHINE;
	}

	// Check ELF version
	if (elf_hdr->e_version != EV_CURRENT)
	{
		return ELF_ERROR_BAD_VERSION;
	}

	// Check the entry point
	if (elf_hdr->e_entry == 0)
	{
		panic("ELF: Bad entry point");
	}

	/* Set this page table up to a default state. Clean up any
	 * pages left over from the old process, we're replacing it.
	 */
	virt_reset_table(page_table);

	// Go through the program sections and load them
	const ELF64_Phdr* phdr_table = (const ELF64_Phdr*)(elf_address + elf_hdr->e_phoff);
	const uint64_t phdr_table_size = elf_hdr->e_phnum;

	for (uint64_t i = 0; i < phdr_table_size; ++i)
	{
		const ELF64_Phdr* cur_hdr = &phdr_table[i];

		if (cur_hdr->p_type != PT_LOAD)
		{
			continue;
		}

		//else if (cur_hdr->p_type == PT_DYNAMIC)
		if ((cur_hdr->p_vaddr + cur_hdr->p_memsz) >= KERNEL_BASE)
		{
			// Problem, this process wants to load into kernel memory
			return ELF_ERROR_BAD_VADDR;
		}

		uint64_t page_size = PAGE_SMALL_SIZE;
		uint64_t page_size_type = PAGE_SMALL;
		if (cur_hdr->p_memsz >= PAGE_LARGE_SIZE)
		{
			page_size = PAGE_LARGE_SIZE;
			page_size_type = PAGE_LARGE;
		}

		const uint64_t num_pages = cur_hdr->p_memsz / page_size + 1;
		/*kprintf("Num pages: %u\n", num_pages);
		kprintf("Memsz: 0x%x\n", cur_hdr->p_memsz);
		kprintf("Page SZ: 0x%x\n", page_size);
		*/
		uint64_t load_offset = cur_hdr->p_offset;
		uint64_t amount_left = cur_hdr->p_filesz;
		uint64_t vaddr = cur_hdr->p_vaddr;
		for (uint64_t i = 0; i < num_pages; ++i)
		{
			//kprintf("VADDR: 0x%x\n", vaddr);
			uint64_t memory_address;
			if (!virt_map_page(page_table, vaddr, 
						PG_FLAG_RW | PG_FLAG_USER, page_size_type, 
						&memory_address))
			{
				panic("ELF: Failed to map page");
			}

			const uint64_t copy_amount = page_size < amount_left ? page_size : amount_left;

			if (amount_left > 0)
			{
				memcpy(PHYS_TO_VIRT(memory_address), 
						(void*)(elf_address + load_offset), copy_amount);

				// Clear the remaining space
				memclr(PHYS_TO_VIRT(memory_address+copy_amount), page_size-copy_amount);
				amount_left -= copy_amount;
			}
			else
			{
				memclr(PHYS_TO_VIRT(memory_address), page_size);
			}

			load_offset += copy_amount;
			vaddr += copy_amount;
		}
	}

#ifdef BIKESHED_X86_64

	// Okay now setup the stack
	#define USER_STACK_LOCATION 0x2000000					
	#define USER_STACK_SIZE 0x4000

	uint64_t address = USER_STACK_LOCATION-USER_STACK_SIZE;
	uint64_t memory_address;
	for (uint64_t i = 0; i < 4; ++i)
	{
		if (!virt_map_page(page_table, address, 
					PG_FLAG_RW | PG_FLAG_USER, PAGE_SMALL, 
					&memory_address))
		{
			panic("ELF: Failed to allocate stack");
		}

		memclr(PHYS_TO_VIRT(memory_address), PAGE_SMALL_SIZE);
		address += PAGE_SMALL_SIZE;
	}

	pcb->context = (Context*)(USER_STACK_LOCATION - sizeof(Context));
	
	// The last memory_address will be the top of the stack!
	kprintf("Memory address: 0x%x\n", memory_address);
	kprintf("Virt address: 0x%x\n", PHYS_TO_VIRT(memory_address));
	Context* context = (Context*)PHYS_TO_VIRT(memory_address+0x1000-sizeof(Context));
	kprintf("Context address: 0x%x\n", context);
	context->IP = elf_hdr->e_entry;
	context->SP = USER_STACK_LOCATION;//-0x10;
	context->BP = USER_STACK_LOCATION;
	context->FLAGS = DEFAULT_EFLAGS;
	context->cs = GDT_CODE_SEG;
	context->ss = GDT_DATA_SEG;
	/*context->rax = 0x12345678;
	context->rbx = 0xABCDEF01;
	context->rcx = 0xA0A0A0A0;
	context->rdx = 0xEFEFEFEF;
	context->rsi = 0x12312313;
	*/
	
#else
#error "ELF Loader not implemented on this architecture"
#endif
	
	// Otherwise we're good
	return ELF_NO_ERROR;
}
