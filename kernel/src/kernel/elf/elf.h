#ifndef __KERNEL_ELF_H__
#define __KERNEL_ELF_H__

#include "safety.h"
#include "inttypes.h"

typedef uint64_t Elf64_Addr; // Unsigned program address
COMPILE_ASSERT(sizeof(Elf64_Addr) == 8);

typedef uint64_t Elf64_Off; // Unsigned file offset
COMPILE_ASSERT(sizeof(Elf64_Addr) == 8);

typedef uint16_t Elf64_Half; // Unsigned medium integer
COMPILE_ASSERT(sizeof(Elf64_Half) == 2);

typedef uint32_t Elf64_Word; // Unsigned integer
COMPILE_ASSERT(sizeof(Elf64_Word) == 4);

typedef int32_t Elf64_Sword; // Signed integer
COMPILE_ASSERT(sizeof(Elf64_Sword) == 4);

typedef uint64_t Elf64_Xword; // Unsigned long integer
COMPILE_ASSERT(sizeof(Elf64_Xword) == 8);

typedef int64_t Elf64_Sxword; // Signed long integer
COMPILE_ASSERT(sizeof(Elf64_Sxword) == 8);

typedef uint8_t ELF_Error;

#define ELF_NO_ERROR 0
#define ELF_ERROR_BAD_MAGIC 1
#define ELF_ERROR_BAD_CLASS 2
#define ELF_ERROR_BAD_ENDIAN 3
#define ELF_ERROR_BAD_ABI 4
#define ELF_ERROR_BAD_TYPE 5

#define ELF_DATA2_LSB 1
#define ELF_DATA2_MSB 2
#define ELF_OSABI_SYSV 0

//-----------------------------------------------------------------------------
// TODO these are architecture specific
//-----------------------------------------------------------------------------

#define ELF_DATA2 ELF_DATA2_LSB
#define ELF_ABI ELF_OSABI_SYSV
//-----------------------------------------------------------------------------

#define El_MAG0 0x7F
#define El_MAG1 'E'
#define El_MAG2 'L'
#define El_MAG3 'F'

#define ELF_CLASS_64 2
#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4

#define ELF_CLASS_OFF 4
#define ELF_DATA_OFF 5
#define ELF_VERSION_OFF 6
#define ELF_OSABI_OFF 7
#define ELF_ABIVERSION_OFF 8
#define ELF_PAD_OFF 9
#define ELF_NIDENT_SIZE 16

typedef struct
{
	uint8_t 	e_ident[16]; 	/* ELF idenfication */	
	Elf64_Half	e_type; 		/* Object file type */	
	Elf64_Half	e_machine;		/* Machine type */
	Elf64_Word	e_version;		/* Object file version */
	Elf64_Addr	e_entry;		/* Entry point address */
	Elf64_Off	e_phoff;		/* Program header offset */
	Elf64_Off	e_shoff;		/* Section header offset */
	Elf64_Word	e_flags;		/* Processor-specific flags */
	Elf64_Half	e_hsize;		/* ELF header size */
	Elf64_Half	e_phentsize;	/* Size of program header entry */
	Elf64_Half	e_phnum;		/* Number of program header entries */
	Elf64_Half	e_shentsize;	/* Size of section header entry */
	Elf64_Half	e_shnum;		/* Number of section header entries */
	Elf64_Half	e_shstrndx;		/* Section name string table index */
} ELF64_Ehdr;

/* Create a new process from an ELF file.
 *
 * Parameters:
 *    elf_file - A pointer to a region of memory that contains the
 *               elf file.
 * Returns:
 *    1 if successfully created process, 0 otherwise.
 */
ELF_Error elf_create_process(void* elf_file);

#endif
