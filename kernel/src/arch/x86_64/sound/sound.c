#include "sound.h"

#include "arch/x86_64/textmode.h"

#include "arch/x86_64/support.h"
#include "arch/x86_64/kprintf.h"
#include "arch/x86_64/pci/pci.h"
#include "arch/x86_64/virt_memory/paging.h"
#include "arch/x86_64/interrupts/interrupts.h"

#include "kernel/klib.h"
#include "kernel/alloc/alloc.h"
#include "kernel/data_structures/watermark.h"

#define PCI_HDCTL 0x40
#define HDA_TCSEL 0x44

#define HDBARL 0x10
#define HDBARU 0x14
#define GCAP 0x0
#define GCTL (0x8/sizeof(uint32_t))
#define WAKEEN (0xC/sizeof(uint16_t))
#define STATESTS (0xE/sizeof(uint16_t))
#define INTCTL (0x20/sizeof(uint32_t))
#define IMM_CMD (0x60/sizeof(uint32_t))
#define IMM_RESP (0x64/sizeof(uint32_t))
#define IRS (0x68/sizeof(uint16_t))
#define INTSTS (0x24/sizeof(uint32_t))

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

#define VERB_GET_PARAM 0xF00
#define VERB_SET_POWER_STATE 0x705

#define PARAM_VENDOR_ID 0x0

#define HDA_MEM_LOC  0xFFFFFFFFF0000000
#define HDA_RING_LOC 0xFFFFFFFFF0004000

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
	uint32_t* base;
	uint64_t size;
	uint64_t num_entries;
} CORB;

typedef struct
{
	uint32_t response;
	uint32_t resp_ex;
} RIRB_Response;

COMPILE_ASSERT(sizeof(RIRB_Response) == 8);

typedef struct
{
	RIRB_Response* base;
	uint64_t size;
	uint64_t num_entries;
	uint32_t read_ptr;
} RIRB;

static CORB hda_corb;
static RIRB hda_rirb;

#define CORB_ENT_SIZE 4
#define CORB_ALIGN 128
#define CORB_DMA_RUN 0x2

#define RIRB_ENT_SIZE 8
#define RIRB_ALIGN 128
#define RIRB_DMA_RUN 0x2

const pci_config_t* pci_hda_config;
static volatile uint32_t* hda_ptr_32 = (volatile uint32_t*) HDA_MEM_LOC;
static volatile uint16_t* hda_ptr_16 = (volatile uint16_t*) HDA_MEM_LOC;
static volatile uint8_t*  hda_ptr_8  = (volatile uint8_t*)  HDA_MEM_LOC;
static uint16_t codecs_available = 0;

static void hda_disable_corb_dma()
{
	hda_ptr_8[CORBCTL] &= ~CORB_DMA_RUN;
	while (hda_ptr_8[CORBCTL] & CORB_DMA_RUN);
}

static void hda_enable_corb_dma()
{
	hda_ptr_8[CORBCTL] |= CORB_DMA_RUN;
}

static void hda_disable_rirb_dma()
{
	hda_ptr_8[RIRBCTL] &= ~RIRB_DMA_RUN;
	while (hda_ptr_8[RIRBCTL] & RIRB_DMA_RUN);
}

static void hda_enable_rirb_dma()
{
	hda_ptr_8[RIRBCTL] |= RIRB_DMA_RUN;
}

static void hda_reset_controller()
{
	// Make sure the DMA controllers are not running
	hda_disable_corb_dma();
	hda_disable_rirb_dma();

	hda_ptr_32[GCTL] &= ~0x1;       // Put the controller into reset mode
	while (hda_ptr_32[GCTL] & 0x1); // Wait for it to reset
	hda_ptr_32[GCTL] |= 0x1;        // Enable the controller
	while ((hda_ptr_32[GCTL] & 0x1) == 0); // Wait for it to finish

	// The STATESTS register will be set with whatever codecs had a state
	// change when the controller was reset. Meaning which codecs are present.
	codecs_available = hda_ptr_16[STATESTS];

	kprintf("Codecs available: 0b%b\n", codecs_available);
}

/* Figure out how much space is left in a ring buffer.
 * Assumes that the readptr is always behind the writeptr.
 *
 * Returns:
 *    How much space is left in the buffer based on the 
 *    total buffer size, readptr and writeptr.
 */
static uint16_t ring_buffer_size(const uint16_t readptr, 
		const uint16_t writeptr, const uint16_t size)
{
	// readptr == writeptr when buffer is empty	
	// readptr is always behind the writeptr
	if (readptr > writeptr)
	{
		return readptr - writeptr;
	}
	else
	{
		return size - (writeptr - readptr);
	}
}

/* Check if the RIRB has any responses waiting to be read
 *
 * Returns:
 *    1 - if a response is available, 0 otherwise
 */
static uint8_t rirb_has_responses()
{
	// Check if the current read ptr is different than the write pointer
	return (hda_rirb.num_entries - ring_buffer_size(hda_rirb.read_ptr, 
			hda_ptr_16[RIRBWP], hda_rirb.num_entries)) > 0;
}

/* Read a response from the RIRB. Assumes rirb_has_responses() was called
 * prior to this method.
 *
 * Side Effects:
 *    Increments the read pointer.
 *
 * Returns:
 *    The response from the RIRB.
 */
static RIRB_Response rirb_read_response()
{
	// Hardware keeps writing to this so we just use our read ptr
	// TODO - check to make sure things are in sync
	uint32_t read_ptr = hda_rirb.read_ptr;
	const RIRB_Response response = hda_rirb.base[read_ptr];
	read_ptr = (read_ptr + 1) % hda_rirb.num_entries;
	hda_rirb.read_ptr = read_ptr;

	return response;
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

	kprintf("CORB SIZE: %u\n", num_entries);

	// Allocate the buffer areas
	hda_corb.size = num_entries*CORB_ENT_SIZE;
	hda_corb.base = (uint32_t*)HDA_RING_LOC;
	hda_corb.num_entries = num_entries;

	// Make sure the CORB DMA engine is off
	hda_disable_corb_dma();

	// Setup the other CORB registers
	hda_ptr_32[CORBLBASE] = (uint64_t)hda_corb.base & 0xFFFFFFFF;	
	hda_ptr_32[CORBUBASE] = ((uint64_t)hda_corb.base >> 32) & 0xFFFFFFFF;

	kprintf("CORBL: 0x%x\nCORBU: 0x%x\n", hda_ptr_32[CORBLBASE], hda_ptr_32[CORBUBASE]);

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

	kprintf("RIRB SIZE: %u\n", num_entries);

	// Allocate the buffer areas
	hda_rirb.size = num_entries*RIRB_ENT_SIZE;
	hda_rirb.base = (RIRB_Response*)(HDA_RING_LOC+2048);
	hda_rirb.num_entries = num_entries;
	hda_rirb.read_ptr = 0;

	// Make sure the RIRB DMA engine is off
	hda_disable_rirb_dma();

	// Setup the other RIRB registers
	hda_ptr_32[RIRBLBASE] = (uint64_t)hda_rirb.base & 0xFFFFFFFF;
	hda_ptr_32[RIRBUBASE] = ((uint64_t)hda_rirb.base >> 32) & 0xFFFFFFFF;

	kprintf("RIRBL: 0x%x\nRIRBU: 0x%x\n", hda_ptr_32[RIRBLBASE], hda_ptr_32[RIRBUBASE]);

	// Clear the response count
	hda_ptr_16[RINTCNT] = 1;

	// Reset the write pointer
	hda_ptr_16[RIRBWP] |= 0x8000;
}

/* Sends a command to the corb. If there is no space available it will sit in
 * a busy loop until space becomes available.
 *
 * Side Effects:
 *    Increments the write pointer.
 *    Stores command in CORB.
 */
static void corb_send_command(const uint32_t command)
{
	uint16_t corb_wp = hda_ptr_16[CORBWP];
	uint16_t corb_rp = hda_ptr_16[CORBRP];

	kprintf("\nWP: %u - RP: %u\n", corb_wp, corb_rp);

	uint16_t space_left = ring_buffer_size(corb_rp, corb_wp, hda_corb.num_entries);

	// TODO possibly send 0x0h commands occasionally for unsolicited responses
	kprintf("CORB: space left: %u\n", space_left);

	while (space_left < 1)
	{
		// Just idle, need better method
		corb_rp = hda_ptr_16[CORBRP];
		space_left = ring_buffer_size(corb_rp, corb_wp, hda_corb.num_entries);
	}

	corb_wp = (corb_wp + 1) % hda_corb.num_entries;	
	hda_corb.base[corb_wp] = command;
	hda_ptr_16[CORBWP] = corb_wp;

	corb_wp = hda_ptr_16[CORBWP];
	corb_rp = hda_ptr_16[CORBRP];

	kprintf("WP: %u - RP: %u\n", corb_wp, corb_rp);
}

static uint32_t hda_send_immediate_command(uint32_t verb)
{
	// Wait for the immediate command to not be busy
	while (hda_ptr_16[IRS] & 0x1);

	// There was a response waiting, but we're going to clear it
	// because it's for an older response
	if (hda_ptr_16[IRS] & 0x2)
	{
		// Clear it
		hda_ptr_16[IRS] |= 0x2;
	}

	// Put the command in the immediate command register
	hda_ptr_32[IMM_CMD] = verb;	

	kprintf("IRS 1: %u\n", hda_ptr_16[IRS]);

	// Send the command
	hda_ptr_16[IRS] |= 0x1;

	kprintf("IRS 2: %u\n", hda_ptr_16[IRS]);
	// Wait for the command to complete
	while (hda_ptr_16[IRS] & 0x1);
	while (!(hda_ptr_16[IRS] & 0x2));	

	kprintf("IRS 3: %u\n", hda_ptr_16[IRS]);
	// Read the response
	const uint32_t response = hda_ptr_32[IMM_RESP];

	// Clear the response bit
	hda_ptr_16[IRS] |= 0x2;

	kprintf("IRS 4: %u\n", hda_ptr_16[IRS]);

	return response; 
}

static void hda_enumerate_codecs()
{
	uint8_t cad = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (codecs_available & (1 << i))
		{
			break;
		}
		++cad;
	}

	uint32_t verb = (cad << 28) | 0xF0000;
	corb_send_command(verb);
	while (!rirb_has_responses());
	RIRB_Response resp = rirb_read_response();

	kprintf("Response: 0x%x - 0x%x\n", 
			resp.response, resp.resp_ex);

	verb = (cad << 28) | 0xF0004;
	corb_send_command(verb);
	while (!rirb_has_responses());
	resp = rirb_read_response();

	kprintf("Response: 0x%x - 0x%x\n", 
			resp.response, resp.resp_ex);
}	

static void hda_interrupt_handler(uint64_t vector, uint64_t error)
{
	UNUSED(vector);
	UNUSED(error);

	kprintf("GOT HDA INTERRUPT!\n");
	__asm__ volatile("hlt");

	//pic_acknowledge(vector);
}

static void hda_setup_streams()
{
	// Read the GCAP register to see how many streams this device supports
	const uint16_t gcap = hda_ptr_16[GCAP];

	const uint8_t num_output_streams = (gcap >> 12) & 0xF;
	const uint8_t num_input_streams = (gcap >> 8)  & 0xF;

	ASSERT(num_output_streams >= 1);

	kprintf("Supports: %u output streams %u input streams\n", num_output_streams, num_input_streams);

	const uint8_t bdl_64_bit = gcap & 0x1;
	kprintf("Supports 64-bit addressing: %u\n", bdl_64_bit);

	// Setup the data buffers for the streams
}

static uint8_t find_custom(const pci_config_t* config)
{
	return config->vendor_id == 0x8086 &&
		   config->base_class == 0x4 &&
		   config->sub_class == 0x3;
}

void sound_init()
{
	kprintf("Initializing sound\n");
	pci_init();

	// Try to find the correct PCI device
	pci_hda_config = pci_find_custom(find_custom);
	if (pci_hda_config == NULL)
	{
		panic("Failed to find HDA device");
	}
	ASSERT(pci_hda_config->header_type == 0);
	const header_type_0* pci_hda_hdr = &pci_hda_config->h_type.type_0;

	kprintf("Found PCI Device:\n");
	kprintf("Vendor: 0x%x - Device: 0x%x - Rev: 0x%x\n",
		pci_hda_config->vendor_id, pci_hda_config->device_id, pci_hda_config->revision);
	kprintf("Subsytem Vendor: 0x%x - Subsystem ID: 0x%x\n",
		pci_hda_hdr->subsystem_vendor_id, pci_hda_hdr->subsystem_id);
	kprintf("Header Type: %d\n", pci_hda_config->header_type);
	for (int i = 0; i < 6; ++i)
	{
		kprintf("BAR%d: 0x%x - Size: 0x%x\n", i, 
				pci_hda_hdr->bar_address[i], pci_hda_hdr->bar_sizes[i]);
	}
	kprintf("Interrupt Line: %u\n", pci_hda_hdr->interrupt_line);
	kprintf("Interrupt Pin: %u\n", pci_hda_hdr->interrupt_pin);

	// Figure out the location
	const uint32_t pci_addr_lo = pci_config_read_l(pci_hda_config, HDBARL);
	const uint32_t pci_addr_hi = pci_config_read_l(pci_hda_config, HDBARU);

	// Allocate space
	ASSERT(PAGE_SMALL_SIZE == 0x1000);
	ASSERT(pci_hda_hdr->bar_sizes[0] == 0x4000);
	virt_map_phys_range(kernel_table, HDA_MEM_LOC, 
			((uint64_t)pci_addr_hi << 32) | (pci_addr_lo & (~0xFFF)),
			PG_FLAG_RW | PG_FLAG_PCD, PAGE_SMALL, 4);
	virt_map_page(kernel_table, HDA_RING_LOC,
			PG_FLAG_RW | PG_FLAG_PCD, PAGE_SMALL, NULL);
	memclr((void*)HDA_RING_LOC, PAGE_SMALL_SIZE);

	// Enable memory writes
	uint32_t command = pci_config_read_w(pci_hda_config, PCI_COMMAND);
	command |= PCI_CMD_MEMORY | PCI_CMD_MASTER;
	pci_config_write_w(pci_hda_config, PCI_COMMAND, command);

	// Make sure the interrupts are all set
	const uint8_t interrupt_line = pci_hda_hdr->interrupt_line;			
	ASSERT(interrupt_line <= 15);
	if (interrupt_line >= 8)
	{
		uint8_t old_mask = _inb(PIC_SLAVE_IMR_PORT);
		kprintf("OLD S MASK: 0b%b\n", old_mask);
		_outb(PIC_SLAVE_IMR_PORT, (~(1 << (interrupt_line - 8))) & old_mask);
		kprintf("NEW S MASK: 0b%b\n", _inb(PIC_SLAVE_IMR_PORT));
	}
	else
	{
		uint8_t old_mask = _inb(PIC_MASTER_IMR_PORT);
		kprintf("OLD M MASK: 0b%b\n", old_mask);
		_outb(PIC_MASTER_IMR_PORT, (~(1 << interrupt_line)) & old_mask);
		kprintf("NEW M MASK: 0b%b\n", _inb(PIC_MASTER_IMR_PORT));
	}
	interrupts_install_isr(interrupt_line+0x20, hda_interrupt_handler);

	kprintf("Status: 0b%b\n", pci_config_read_w(pci_hda_config, PCI_STATUS));
	kprintf("Command: 0b%b\n", pci_config_read_w(pci_hda_config, PCI_COMMAND));

	// Reset the controller
	hda_reset_controller();

	// Setup the ring buffers
	hda_setup_corb();
	hda_setup_rirb();

	// Enable the DMA engines
//	hda_enable_corb_dma();
//	hda_enable_rirb_dma();

	// Figure out what we have
//	hda_enumerate_codecs();

	// Try a command
	uint8_t cad = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (codecs_available & (1 << i))
		{
			break;
		}
		++cad;
	}

	uint32_t verb = (cad << 28) | 0xF0004;
	uint32_t response = hda_send_immediate_command(verb);
	kprintf("Response: 0x%x\n", response);
}
