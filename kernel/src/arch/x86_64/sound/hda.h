#ifndef __X86_64_SOUND_HDA_H__
#define __X86_64_SOUND_HDA_H__

#include "inttypes.h"
#include "kernel/data_structures/linkedlist.h"

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
#define CORBST 0x4D

#define RIRBSIZE 0x5E
#define RIRBLBASE (0x50/sizeof(uint32_t))
#define RIRBUBASE (0x54/sizeof(uint32_t))
#define RINTCNT (0x5A/sizeof(uint16_t))
#define RIRBWP (0x58/sizeof(uint16_t))
#define RIRBCTL 0x5C
#define RIRBSTS 0x5D

#define DPLBASE (0x70/sizeof(uint32_t))
#define DPUBASE (0x74/sizeof(uint32_t))

#define SDCTL0 (0x80/sizeof(uint32_t))

#define VERB_GET_PARAM 0xF00
#define VERB_SET_POWER_STATE 0x705

#define PARAM_VENDOR_ID 0x0

#define HDA_MEM_LOC  0xFFFFFFFFF0000000
#define HDA_RING_LOC 0xFFFFFFFFF0006000
#define HDA_DMA_LOC  0xFFFFFFFFF000A000
#define HDA_STREAM_BASE 0xFFFFFFFFF000C000
#define HDA_STREAM_DATA_BASE 0xFFFFFFFFF0200000

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

typedef struct
{
	// The node number of this widget
	uint8_t node;

	// Capabilities
	uint8_t type;
	uint8_t delay;
	uint8_t channel_count;
	uint8_t cp_caps;
	uint8_t lr_swap;
	uint8_t power_control;
	uint8_t digital;
	uint8_t conn_list;
	uint8_t unsolicit_cap;
	uint8_t processing_widget;
	uint8_t stripe;
	uint8_t format_override;
	uint8_t amp_param_override;
	uint8_t out_amp_present;
	uint8_t in_amp_present;

	// Supported PCMs
	// Supported stream formats
	struct 
	{
		// Pin Capabilities
		uint8_t high_bit_rate;
		uint8_t display_port;
		uint8_t eapd_capable;
		uint8_t vref_control;
		uint8_t hdmi;
		uint8_t balanced_io;
		uint8_t input_capable;
		uint8_t output_capable;
		uint8_t headphone_drive_cap;
		uint8_t presence_detectable;
		uint8_t trigger_required;
		uint8_t imped_sense_capable;
	} pin_caps;

	struct
	{
		uint8_t mute_capable;
		uint8_t step_size;
		uint8_t num_steps;
		uint8_t offset;
	} in_amp_caps;

	struct
	{
		uint8_t mute_capable;
		uint8_t step_size;
		uint8_t num_steps;
		uint8_t offset;
	} out_amp_caps;

	// Connection list length
	struct
	{
		uint8_t long_form;
		uint8_t length;
		linked_list_t lst_conn;
	} conn_info;

	// Volume knob capabilities
	struct
	{
		uint8_t delta;
		uint8_t num_steps;
	} volume_knob;

	// Default configuration
	struct
	{
		uint8_t port_connectivity;
		uint8_t location;
		uint8_t default_device;
		uint8_t connection_type;
		uint8_t color;
		uint8_t misc;
		uint8_t default_assoc;
		uint8_t sequence;
	} default_config;

} AudioWidget;

typedef struct
{
	linked_list_t path;
} AudioOutputPath;

typedef struct
{
	uint8_t codec;
	uint8_t node;
	linked_list_t lst_widgets;
	linked_list_t lst_outputs;
} AudioFunctionGroup;

typedef struct
{
	uint8_t number;
	uint64_t out_sdctl_offset;
	uint64_t in_sdctl_offset;
	struct
	{
		// 0 - 48kHz
		// 1 - 44.1kHz
		uint8_t sample_base_rate;
		// 000 - 48kHz, 44.1kHz or less
		// 001 - x2 (96Hz, 88.2Hz, 32kHz)
		// 010 - x3 (144kHz)
		// 011 - x4 (192kHz, 176.4kHz)
		// --- - Reserved for the rest
		uint8_t sample_base_rate_multiple;
		// 000 - Divide by 1 (48kHz, 44.1kHz)
		// 001 - Divide by 2 (24kHz, 22.05kHz)
		// 010 - Divide by 3 (16kHz, 32kHz)
		// 011 - Divide by 4 (11.025kHz)
		// 100 - Divide by 5 (9.6kHz)
		// 101 - Divide by 6 (8kHz)
		// 110 - Divide by 7
		// 111 - Divide by 8 (6kHz)
		uint8_t sample_base_rate_devisor;
		// 000 - 8 bits  8-bit container/16-bit boundaries
		// 001 - 16-bits 16-bit container/16-bit boundaries
		// 010 - 20-bits 32-bit container/32-bit boundaries
		// 011 - 24-bits 32-bit container/32-bit boundaries
		// 100 - 32-bits 32-bit containers/32-bit boundaries
		uint8_t bits_per_sample;
		// 0000 - 1
		// 0001 - 2
		// ....
		// 1111 - 16
		uint8_t number_of_channels;
	} descriptor;

	struct
	{
		uint64_t data_address;
		uint64_t data_phys_address;
		uint64_t data_length;
		uint64_t bdl_buffer_address;
		uint64_t bdl_buffer_phys_address;
		uint64_t bdl_buffer_length;
	} bdl_info;
} Stream;

#define CORB_ENT_SIZE 4
#define CORB_ALIGN 128
#define CORB_DMA_RUN 0x2

#define RIRB_ENT_SIZE 8
#define RIRB_ALIGN 128
#define RIRB_DMA_RUN 0x2

#endif
