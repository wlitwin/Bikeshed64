#include "sound.h"

#include "arch/x86_64/textmode.h"

#include "arch/x86_64/support.h"
#include "arch/x86_64/kprintf.h"
#include "arch/x86_64/pci/pci.h"
#include "arch/x86_64/virt_memory/paging.h"
#include "arch/x86_64/interrupts/interrupts.h"

#include "kernel/alloc/alloc.h"
#include "kernel/data_structures/watermark.h"

#define HDBARL 0x10
#define HDBARU 0x14
#define HDA_MEM_ENABLE 0x2
#define GCAP 0x0
#define GCTL (0x8/sizeof(uint32_t))
#define WAKEEN (0xC/sizeof(uint16_t))
#define STATESTS (0xE/sizeof(uint16_t))
#define INTCTL (0x20/sizeof(uint32_t))

#define CORBSIZE 0x4E
#define CORBLBASE (0x40/sizeof(uint32_t))
#define CORBUBASE (0x44/sizeof(uint32_t))
#define CORBWP (0x48/sizeof(uint16_t))
#define CORBRP (0x4A/sizeof(uint16_t))
#define CORBCTL 0x4C

#define RIRBSIZE 0x5E
#define RIRBLBASE (0x50/sizeof(uint32_t))
#define RIRBUBASE (0x54/sizeof(uint32_t))
#define RINTCNT (0x5A/sizeof(uint16_t))
#define RIRBWP (0x58/sizeof(uint16_t))
#define RIRBCTL 0x5C

#define HDA_MEM_LOC 0xFFFFFFFFFE000000

// Information taken from here:
// http://www.intel.com/content/dam/www/public/us/en/documents/product-specifications/high-definition-audio-specification.pdf

// DMA position must be aligned to a 128-byte boundary (bottom 7-bits 0)
// 
// struct stream_pos
// {
//    uint32_t position;
//    uint32_t reserved;
// }
//
// struct DMA_Pos
// {
//    struct stream_pos s_pos[num_streams];
// }

// Buffer Descriptor List (BDL), must be at least 2 entries, max of 256
// must be aligned to a 128-byte boundary. Should not be modified unless
// the run bit is 0
//
// BDLEs must start on a 128-byte boundary, and length must be an integer
// number of words
//
// struct BDL_Entry
// {
//    uint64_t address; // 64-bit address of the buffer
//    uint32_t length;  // 32-bit length of the buffer (bytes) must be >= 1 word
//    uint32_t ioc;     // High bit = interrupt on completion, low bits reserved
// }
//
// struct BDL
// {
//    BDL_Entry entries[num_bdles];
// }

// Command Output Ring Buffer (CORB). Length == CORBSIZE register. Must start
// on a 128-byte boundary
//
// Codec verbs:
//     31:28    -  27:20  -   19:0
//   Codec Addr - Node Id - Verb Payload
//
// struct CORB
// {
//    uint32_t verb[num_verbs];
// }

// Response Input Ring Buffer (RIRB). Length == RIRBSIZE register. Must start
// on a 128-byte boundary
//
// Solicited Response:
//     31:0
//   Response
//
// Unsolicited Response:
//     31:26 -  25:21  -        20:0
//      Tag  - Sub Tag - Vendor Specific Contents
//
// struct Response
// {
//    uint32_t resp;
//    uint32_t resp_extended;
// }
//
// struct RIRB
// {
//    struct Response resp[num_responses];
// }

// Stream Format Structure - Does not appear in memory
// Bits  - Meaning
//  15   - Type
//  14   - Sample Rate Base
// 13:11 - Sample Base Rate Multiple
// 10:8  - Sample Base Rate Divisor
//   7   - Reserved
//  6:4  - Bits per sample
//  3:0  - Number of channels

// Initialization
// ==============
// When the controller is reset the CRST (offset 0x8, bit 0) bit will be 0. When
// a 1 is written to it it will start, software should then wait until it's set
// to 1 before continuing.
//
// Check the WAKEEN, STATESTS and ther RSM registers as they maintain state across
// power resets
//
// Codec Discovery
// ===============
// STATESTS bits will be set to indicate addresses of the codecs present
//
// Software must wait 521us after reading CRST as a 1 before assuming all the
// codecs have been registered.
//
// Can use CIE bit in INTCTL register to be notified when status has changed
//
// Codec Command and Control
// =========================
// Configure the CORB and RIRB buffers
//
// CORB - Size of the CORB is programmable to 2, 16, or 256 entries using the 
//        CORBSIZE controller register. Choose based on CORBSZCAP field. Generally
//        choose the 256 option.
//
//        Hardware maintains two pointers. Write Pointer (WP) and Read Pointer (RP)
//        WP = (set by software) last valid command in CORB
//        RP = (set by hardware) last command fetched in CORB
//        Both measure the offset into the buffer, need to multiply by 4
//
// Add commands into the CORB by (WP+1)*4, then update WP. When first initialized
// WP = 0, so first command is (0+1)*4 = 4, and then WP = 1
//
// First make sure the CORBRUN bit in CORBCTL is 0
// CORBBASE = base of allocated memory
// use CORBRPRST bit to clear RP
// write 0 to WP
//
// RIRB - Size of RIRB is programmable to 2, 16, or 256 entries. Must be aligned
//        to 128-byte boundary. There is no RP register, software must maintain
//        this. WP is kept in hardware to indicate the last response written.
//
//        Two ways software can be notified of a response. First is by interrupt
//        after N responses, or when an empty response slot is found.
//
//        Initialize the RIRB - set RIRBSIZE, RIRBUBASE, RIRBLBASE

// Stream Management
// =================
// Connects codec with system memory, 1 stream = 1 DMA
//
// Samples are left-justified (LSBs are 0) smallest container which fits the
// sample are used (so 24-bit sample = 32-bit container). Samples must be
// naturally aligned in memory.
//
// Block - set of samples to be rendered at one point in time. A block has a
//         size of (container size * number of channels)
//
// Standard output rate is 48kHz. Multiple blocks to be transmitted at a time
// are organized into packets. Packets are collected in memory into buffers,
// which are commonly a whole page of memory. Each buffer must have an integer
// number of samples. But blocks and packets can be split across multiple buffers.
//
// Starting Streams
// ================
// Determine stream parameters - sample rate, bit depth, number of channels
//
// Data buffer + BDL allocated. Setup BDL appropriately.
//
// Stopping Streams
// ================
// Write a 0 to the RUN bit in the stream descriptor. This bit will not 
// immediately transition to 0. Rather, the DMA engine will stop on the next
// frame ~40us.
//
// Resuming Streams
// ================
// Set the run bit back to 1. First check to make sure it's 0.
//
// Getting Data
// ============
// Can use interrupts to know when the streams have finished or require reading.
//
// ISR - Should only use byte access to read or write the Status register.
//       Should not attempt to write to the stream control register.

// Discovering Codecs
// ==================

typedef struct
{
	uint64_t* base;
	uint64_t size;
} CORB;

typedef CORB RIRB;

static CORB hda_corb;
static RIRB hda_rirb;

#define CORB_ENT_SIZE 4
#define CORB_ALIGN 128
#define CORB_DMA_RUN 0x2

#define RIRB_ENT_SIZE 4
#define RIRB_ALIGN 128
#define RIRB_DMA_RUN 0x2

const pci_config_t* pci_hda_config;
static volatile uint32_t* hda_ptr_32 = (volatile uint32_t*) HDA_MEM_LOC;
static volatile uint16_t* hda_ptr_16 = (volatile uint16_t*) HDA_MEM_LOC;
static volatile uint8_t*  hda_ptr_8  = (volatile uint8_t*)  HDA_MEM_LOC;
static uint16_t codecs_available = 0;

static void* hda_alloc_aligned(uint64_t size, uint64_t align)
{
	void* ptr = water_mark_alloc_align(&kernel_WaterMark, align, size);
	ASSERT(ptr != NULL);
	return ptr;
}

static void hda_interrupt_handler(uint64_t vector, uint64_t code)
{
	UNUSED(vector);
	UNUSED(code);

	// Handle the interrupt
	kprintf("HDA INT!\n");
}

static void hda_reset_controller()
{
	hda_ptr_32[GCTL] &= ~0x1; // Put the controller into reset mode
	while (hda_ptr_32[GCTL] & 0x1); // Wait for it to reset
	hda_ptr_32[GCTL] |= 0x1;  // Enable the controller
	while ((hda_ptr_32[GCTL] & 0x1) == 0); // Wait for the bit to be set

	// Check the WAKEEN register, state is saved across resets
	hda_ptr_16[WAKEEN] &= ~0x7;

	// Check the STATESTS register, state is saved across resets
	//hda_ptr_16[STATESTS] &= ~0x7;
	codecs_available = hda_ptr_16[STATESTS];
}

static void hda_setup_corb()
{
	// Read the and set CORB capability	register
	const uint8_t supported_size = hda_ptr_8[CORBSIZE] >> 4;

	uint32_t num_entries = 0;
	if (supported_size & 0x4)
	{
		// 256 entries
		hda_ptr_8[CORBSIZE] = 0x2;
		num_entries = 256;
	}
	else if (supported_size & 0x2)
	{
		// 16 entries
		hda_ptr_8[CORBSIZE] = 0x1;
		num_entries = 16;
	}
	else
	{
		// 2 entries
		hda_ptr_8[CORBSIZE] = 0x0;
		num_entries = 2;
	}

	// Allocate the buffer areas
	hda_corb.size = num_entries*CORB_ENT_SIZE;
	hda_corb.base = (uint64_t*)hda_alloc_aligned(hda_corb.size, CORB_ALIGN);

	// Make sure the CORB DMA engine is off
	hda_ptr_8[CORBCTL] &= ~CORB_DMA_RUN;
	while (hda_ptr_8[CORBCTL] & CORB_DMA_RUN);

	// Setup the other CORB registers
	hda_ptr_32[CORBLBASE] = (uint64_t)hda_corb.base & 0xFFFFFFFF;	
	hda_ptr_32[CORBUBASE] = ((uint64_t)hda_corb.base >> 32) & 0xFFFFFFFF;

	// Reset the read pointer, need to set it, wait, then reset it
	// and wait to verify that the reset was completed successfully
	hda_ptr_16[CORBRP] |= 0x8000;
	while ((hda_ptr_16[CORBRP] & 0x8000) == 0);
	hda_ptr_16[CORBRP] &= ~0x8000;
	while (hda_ptr_16[CORBRP] & 0x8000);

	// Set the write pointer to 0, no commands
	hda_ptr_16[CORBWP] = 0;
}

static void hda_setup_rirb()
{
	// Read and set the RIRB capability register
	const uint8_t supported_size = hda_ptr_8[RIRBSIZE] >> 4;
	
	uint32_t num_entries = 0;
	if (supported_size & 0x4)
	{
		// 256 entries
		hda_ptr_8[RIRBSIZE] = 0x2;
		num_entries = 256;
	}
	else if (supported_size & 0x2)
	{
		// 16 entries
		hda_ptr_8[RIRBSIZE] = 0x1;
		num_entries = 16;
	}
	else
	{
		// 2 entries
		hda_ptr_8[RIRBSIZE] = 0x0;
		num_entries = 2;
	}

	// Allocate the buffer areas
	hda_rirb.size = num_entries*RIRB_ENT_SIZE;
	hda_rirb.base = (uint64_t*)hda_alloc_aligned(hda_rirb.size, RIRB_ALIGN);

	// Make sure the RIRB DMA engine is off
	hda_ptr_8[RIRBCTL] &= ~RIRB_DMA_RUN;
	while (hda_ptr_8[RIRBCTL] & RIRB_DMA_RUN);

	// Setup the other RIRB registers
	hda_ptr_32[RIRBLBASE] = (uint64_t)hda_rirb.base & 0xFFFFFFFF;
	hda_ptr_32[RIRBUBASE] = ((uint64_t)hda_rirb.base >> 32) & 0xFFFFFFFF;

	// Clear the response count
	hda_ptr_16[RINTCNT] &= ~0xF;

	// Reset the write pointer
	hda_ptr_16[RIRBWP] |= 0x8000;
}

static void hda_setup_streams()
{
	// Read the GCAP register to see how many streams this device supports
	const uint16_t gcap = hda_ptr_16[GCAP];

	const uint8_t num_ouput_streams = (gcap >> 12) & 0xF;
	const uint8_t num_input_streams = (gcap >> 8)  & 0xF;

	kprintf("Supports: %u output streams %u input streams\n", num_ouput_streams, num_input_streams);

	const uint8_t bdl_64_bit = gcap & 0x1;
	kprintf("Supports 64-bit addressing: %u\n", bdl_64_bit);

	// Setup the data buffers for the streams
}

static void hda_enable_interrupts()
{
#define GIE 0x80000000
#define CIE 0x40000000
	hda_ptr_32[INTCTL] |= GIE | CIE;
}

void sound_init()
{
	kprintf("Initializing sound\n");
	pci_init();

	// Try to find the correct PCI device
	pci_hda_config = pci_find_by_class(0x4, 0x3, 0x0);	
	if (pci_hda_config == NULL)
	{
		panic("Failed to find HDA device");
	}

	// We have the PCI device, now we have to map it somewhere
	const uint32_t pci_addr_lo = pci_config_read_l(pci_hda_config, HDBARL);
	const uint32_t pci_addr_hi = pci_config_read_l(pci_hda_config, HDBARU);

	kprintf("HDA ADDR LO: 0x%x\n", pci_addr_lo);
	kprintf("HDA ADDR HI: 0x%x\n", pci_addr_hi);

	// Check if we can relocate anywhere in 64-bit address space
	if ((pci_addr_lo & 0x4) > 0)
	{
		kprintf("Can locate anywhere\n");
	}
	else
	{
		kprintf("Only 32-bit addresses\n");
	}

	// Map it somewhere
	kprintf("Require 0x%x bytes\n", pci_hda_config->h_type.type_0.bar_sizes[0]);
	ASSERT(pci_hda_config->h_type.type_0.bar_sizes[0] == 0x4000);

	// Choose this address for now, we don't really have a good way of giving 
	// out virtual addresses yet...
	ASSERT(PAGE_SMALL_SIZE == 0x1000);
	virt_map_phys_range(kernel_table, HDA_MEM_LOC, pci_addr_lo, PG_FLAG_RW, PAGE_SMALL, 4);

	// Now we can access the registers through this memory mapped address
	// Tell the PCI device we're going to the memory mapped registers	
	const uint16_t pci_cmd = pci_config_read_w(pci_hda_config, PCI_COMMAND);
	pci_config_write_w(pci_hda_config, PCI_COMMAND, pci_cmd | HDA_MEM_ENABLE);

	// Figure out what interrupt this device has been assigned to
	kprintf("Interrupt on line: %u\n", pci_hda_config->h_type.type_0.interrupt_line);
	const uint32_t actual_interrupt = pci_hda_config->h_type.type_0.interrupt_line+0x20;

	interrupts_install_isr(actual_interrupt, hda_interrupt_handler);

	// Now we need to enable the controller
	hda_reset_controller();

	// Enable interrupts from the controller
	hda_enable_interrupts();	

	// Initialize the CORB and RIRB
	hda_setup_corb();
	hda_setup_rirb();

	// Initialize streams
	hda_setup_streams();

	page_up();
}
