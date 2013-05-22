/* For x86 we're going to place the kernel at 0x200000 (2MiB)
 * and place the kernel's stack under it from 0x100000-0x200000.
 *
 * To make things easier and to save TLB space the kernel will
 * use 4MiB pages for it's code/data.
 *
 * Additionally we have a higher half kernel for x86. This means
 * the kernel is at 0xC0000000 in virtual memory. For simplicity
 * everything from 0x100000-0x400000 will be mapped to the range
 * 0xC0000000-0xC0400000.
 *
 * This means the following mapping will be true:
 *
 * 0xC0000000-0xC0100000 -> 0x00000000-0x00100000 (Low Memory)
 * 0xC0100000-0xC0200000 -> 0x00100000-0x00200000 (Kernel Stack)
 * 0xC0200000-0xC0400000 -> 0x00200000-0x00400000 (Kernel Code/Data)
 */

KERNEL_BASE_LOCATION = 0xC0000000 /* Virtual load location */
KERNEL_LOAD_LOCATION = 0x00200000 /* Phyiscal load location */

/* The page directory index for of the virtual load location */
KERNEL_PD_INDEX = KERNEL_BASE_LOCATION >> 22 

/* Page directory entry flags */
PG_PRESENT    = 0x01
PG_READ_WRITE = 0x02
PG_4MIB       = 0x80

KERNEL_FLAGS = (PG_4MIB | PG_READ_WRITE | PG_PRESENT)

/* This is the value we put in the page directory index, this
 * means map the first 4MiB of ram to the virtual location
 * specified by the page directory index we're placing it in.
 */
KERNEL_PD_VALUE = (0x00000000 | KERNEL_FLAGS)

/* The kernel stack will go directly below the kernel's (virt) load
 * location and is set to 1MiB for now.
 */
KERNEL_STACK_LOCATION = KERNEL_BASE_LOCATION + (KERNEL_LOAD_LOCATION - 0x4)

/* The following is the kernel's page directory.
 * index 0 is filled with the KERNEL_PD_VALUE
 * from above so 0-4MiB is identity mapped.
 * Then the rest of the entries up to the kernel's
 * virtual location are filled with 0 to indicate
 * those pages are not present. Then the kernel's
 * virtual location index is filled with the
 * same mapping so it maps:
 *
 *   KERNEL_LOAD_LOCATION -> KERNEL_LOAD_LOCATION + 4MiB
 * to:
 *   0x00000000 -> 0x00400000 
 *
 * Which is where the kernel resides in physical memory.
 */
.data
.align 4096
.globl g_kernel_page_directory
g_kernel_page_directory:
	.long KERNEL_PD_VALUE
	.fill (KERNEL_PD_INDEX - 1), 4, 0
	.long KERNEL_PD_VALUE
	.fill (1024 - KERNEL_PD_INDEX - 1), 4, 0

/* The pre_kernel 'function' will be called by the
 * bootloader. This sets up the kernel's page directory
 * and turns on virtual memory. It also fixes the
 * kernel's stack so it's no longer using the bootloader's
 * stack and initializes the BSS section to 0.
 */
.text
.code32
.align 4096
.globl pre_kernel
pre_kernel:
	/* Turn on paging first, then fixup everything else
	 */

	/* Set CR3 to point to the g_kernel_page_directory */
	movl $(g_kernel_page_directory - KERNEL_BASE_LOCATION), %eax
	movl %eax, %cr3

	/* Set the 4MiB paging bit in CR4. This allows the use 
	 * of 4MiB and 4KiB pages.
	 */
	movl %cr4, %eax
	orl $0x00000010, %eax
	movl %eax, %cr4

	/* Set the paging bit */
	movl %cr0, %eax	
	orl $0x80000000, %eax	
	movl %eax, %cr0

	/* Paging is enabled */

	/* Fix EIP so that it's executing code from the higher address */
	movl $higher_half, %eax
	jmp *%eax
higher_half:

	/* EIP -> 0xC0200000 and higher */

	/* Now that we're in the higher half we can fix the stack pointer */
	movl $KERNEL_STACK_LOCATION, %esp
	movl $KERNEL_STACK_LOCATION, %ebp

	/* ESP/EBP -> 0xC01fffff and lower */

	/* Initialize the BSS section to zeroes */
	movl $sbss, %edi
clear_bss_loop:
	movl $0, (%edi)
	addl $4, %edi
	cmpl $ebss, %edi
	jb clear_bss_loop 

	/* Now we're all ready to jump to the higher half kernel */
	call kmain

	/* Kernel returned, shouldn't happen, but stop the processor anyway */
	cli
kernel_returned:
	hlt
	jmp kernel_returned

