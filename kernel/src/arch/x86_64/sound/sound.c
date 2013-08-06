#include "hda.h"
#include "sound.h"
#include "music.h"

#include "arch/x86_64/textmode.h"

#include "arch/x86_64/support.h"
#include "arch/x86_64/kprintf.h"
#include "arch/x86_64/pci/pci.h"
#include "arch/x86_64/virt_memory/paging.h"
#include "arch/x86_64/interrupts/interrupts.h"

#include "kernel/klib.h"
#include "kernel/alloc/alloc.h"
#include "kernel/data_structures/watermark.h"

static CORB hda_corb;
static RIRB hda_rirb;

static AudioFunctionGroup hda_afg;
static linked_list_t hda_lst_streams;

static uint64_t dma_base_addresses[8];
static uint64_t hda_dma_phys_loc = 0;
static uint64_t hda_ring_phys_loc = 0;
static const pci_config_t* pci_hda_config;
static volatile uint32_t* hda_ptr_32 = (volatile uint32_t*) HDA_MEM_LOC;
static volatile uint16_t* hda_ptr_16 = (volatile uint16_t*) HDA_MEM_LOC;
static volatile uint8_t*  hda_ptr_8  = (volatile uint8_t*)  HDA_MEM_LOC;
static uint16_t codecs_available = 0;
static uint8_t hda_num_output_streams = 0;
static uint8_t hda_num_input_streams = 0;
static volatile StreamReg* hda_stream_regs;

//=============================================================================
// hda_alloc
//
// Allocates permanent memory for the driver. This memory is not meant to be
// free'd, so only use it for memory that should stick around the whole time
// the OS is running. Primarily used for the linked_list_t structure.
//
// Parameters:
//   size - The number of bytes to allocate
//
// Returns:
//   A pointer to the allocated memory, or NULL if could not allocate memory 
//=============================================================================
static void* hda_alloc(const uint64_t size)
{
	void* ptr = water_mark_alloc(&kernel_WaterMark, size);	
	ASSERT(ptr != NULL);
	return ptr;
}

//=============================================================================
// hda_free
//
// This method is not meant to be used and if called it will cause a kernel
// panic. This is only here because the linked_list_t initialization routine
// requires a pointer to a free function.
//
// Parameters:
//   ptr - Not used
//=============================================================================
static void hda_free(void* ptr)
{
	UNUSED(ptr);
	panic("Tried to free an HDA element");
}

//=============================================================================
// hda_disable_corb_dma
//
// Disables the CORB DMA engine. This must be done before updating certain
// registers otherwise a possible corrupt DMA transfer may happen.
//=============================================================================
static void hda_disable_corb_dma()
{
	hda_ptr_8[CORBCTL] &= ~CORB_DMA_RUN;
	while (hda_ptr_8[CORBCTL] & CORB_DMA_RUN);
}

//=============================================================================
// hda_enable_corb_dma
//
// Enables the CORB DMA engine.
//=============================================================================
static void hda_enable_corb_dma()
{
	hda_ptr_8[CORBCTL] |= CORB_DMA_RUN;
}

//=============================================================================
// hda_disable_rirb_dma
//
// Disables the RIRB DMA engine. This must be done before updating certain
// registers otherwise a possible corrupt DMA transfer may happen.
//=============================================================================
static void hda_disable_rirb_dma()
{
	hda_ptr_8[RIRBCTL] &= ~RIRB_DMA_RUN;
	while (hda_ptr_8[RIRBCTL] & RIRB_DMA_RUN);
}

//=============================================================================
// hda_enable_rirb_dma
//
// Enables the RIRB DMA engine.
//=============================================================================
static void hda_enable_rirb_dma()
{
	hda_ptr_8[RIRBCTL] |= RIRB_DMA_RUN;
}

//=============================================================================
// hda_reset_controller
//
// Resets the HDA codec. It performs all the proper initialization steps. 
// After this method has been called the codecs_available variable will hold
// a bitmask of all the available codecs. The bit position is the codecs
// address, so bit 0 == address 0, bit 1 == address 1, etc.
//=============================================================================
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

//=============================================================================
// ring_buffer_size
//
// Figure out how much space is left in a ring buffer. Assumes that the 
// readptr is always behind the writeptr.
//
// Returns:
//   How much space is left in the buffer based on the total buffer size,
//   readptr and writeptr
//=============================================================================
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

//=============================================================================
// rirb_has_responses
//
// Check if the RIRB has any responses waiting to be read
//
// Returns:
//   1 - if a response is available, 0 otherwise
//=============================================================================
static uint8_t rirb_has_responses()
{
	// Check if the current read ptr is different than the write pointer
	return (hda_rirb.num_entries - ring_buffer_size(hda_rirb.read_ptr, 
			hda_ptr_16[RIRBWP], hda_rirb.num_entries)) > 0;
}

//=============================================================================
// rirb_read_response
//
// Read a response from the RIRB. Assumes rirb_has_responses() was called 
// prior to this method.
//
// Returns:
//   The response from the RIRB
//=============================================================================
static RIRB_Response rirb_read_response()
{
	// Hardware keeps writing to this so we just use our read ptr
	// TODO - check to make sure things are in sync
	
	if (hda_ptr_8[RIRBSTS] & 0x1)
	{
		hda_ptr_8[RIRBSTS] |= 0x1;
	}

	uint32_t read_ptr = hda_rirb.read_ptr;
	read_ptr = (read_ptr + 1) % hda_rirb.num_entries;
	const RIRB_Response response = hda_rirb.base[read_ptr];
	hda_rirb.read_ptr = read_ptr;

	return response;
}

//=============================================================================
// hda_setup_corb
//
// Initializes the CORB registers and sets up the CORB so that it's ready to
// accept commands. Before using the CORB make sure the DMA engines are on.
//=============================================================================
static void hda_setup_corb()
{
	kprintf("CORBSZ: 0x%x\n", hda_ptr_8[CORBSIZE]);

	// Make sure the CORB DMA engine is off
	hda_disable_corb_dma();

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

	// Setup the other CORB registers
	hda_ptr_32[CORBLBASE] = (uint64_t)hda_ring_phys_loc & 0xFFFFFFFF;	
	hda_ptr_32[CORBUBASE] = ((uint64_t)hda_ring_phys_loc >> 32) & 0xFFFFFFFF;

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

//=============================================================================
// hda_setup_rirb
//
// Initializes the RIRB registers and sets up the RIRB so that it's ready to
// recieve commands. Before using the RIRB make sure the DMA engines are on.
//=============================================================================
static void hda_setup_rirb()
{
	kprintf("RIRBSZ: 0x%x\n", hda_ptr_8[RIRBSIZE]);

	// Make sure the RIRB DMA engine is off
	hda_disable_rirb_dma();
	
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

	// Setup the other RIRB registers
	const uint64_t rirb_loc = hda_ring_phys_loc + 2048;
	hda_ptr_32[RIRBLBASE] = (uint64_t)rirb_loc & 0xFFFFFFFF;
	hda_ptr_32[RIRBUBASE] = ((uint64_t)rirb_loc >> 32) & 0xFFFFFFFF;

	kprintf("RIRBL: 0x%x\nRIRBU: 0x%x\n", hda_ptr_32[RIRBLBASE], hda_ptr_32[RIRBUBASE]);
	kprintf("RIRB BASE: 0x%x\n", hda_rirb.base);

	// Set the number of responses before an interrupt
	hda_ptr_16[RINTCNT] = 1;

	// Reset the write pointer
	hda_ptr_16[RIRBWP] |= 0x8000;

	hda_ptr_8[RIRBCTL] |= 0x1; // Turn on RIRB interrupts
}

//=============================================================================
// corb_send_command
//
// Sends a command to the CORB. If there is no space availabe it will sit in
// a busy loop until space becomes available.
//
// Parameters:
//   command - The command to send to the codec
//=============================================================================
static void corb_send_command(const uint32_t command)
{
	uint16_t corb_wp = hda_ptr_16[CORBWP] & 0xFF;
	uint16_t corb_rp = hda_ptr_16[CORBRP] & 0xFF;

//	kprintf("\nWP: %u - RP: %u RIRBWP %u\n", corb_wp, corb_rp, hda_ptr_16[RIRBWP]);

	uint16_t space_left = ring_buffer_size(corb_rp, corb_wp, hda_corb.num_entries);

	// TODO possibly send 0x0h commands occasionally for unsolicited responses
//	kprintf("CORB: space left: %u\n", space_left);

	while (space_left < 1)
	{
		// Just idle, need better method
		corb_rp = hda_ptr_16[CORBRP] & 0xFF;
		space_left = ring_buffer_size(corb_rp, corb_wp, hda_corb.num_entries);
	}

	corb_wp = (corb_wp + 1) % hda_corb.num_entries;	
	hda_corb.base[corb_wp] = command;
	hda_ptr_16[CORBWP] = corb_wp & 0xFF;

//	corb_wp = hda_ptr_16[CORBWP];
//	corb_rp = hda_ptr_16[CORBRP];

//	kprintf("WP: %u - RP: %u\n", corb_wp, corb_rp);
}

//=============================================================================
// poll_command
//
// A helper method to synchronously send a command to the codec. It waits 
// until the codec has processed the request and then returns the result.
//
// Parameters:
//   verb - The command to send to the CORB
//
// Returns:
//   The response from the RIRB
//=============================================================================
static RIRB_Response poll_command(uint32_t verb)
{
	corb_send_command(verb);
	while (!rirb_has_responses());
	return rirb_read_response();
}

//=============================================================================
// create_verb
//
// A helper method to create a command suitable for the CORB.
//
// Parameters:
//   cad - The codec address
//   d - Indirect node or direct node reference
//   nid - The node
//   verb - The verb/command
//   data - The parameter for the verb
//=============================================================================
static uint32_t create_verb(uint32_t cad, uint32_t d, uint32_t nid,
		uint32_t verb, uint32_t data)
{
	return (cad << 28) 
		| (d << 27) 
		| (nid << 20) 
		| (verb << 8) 
		| (data & 0xFFFF);
}

//=============================================================================
// hda_send_immediate_command
//
// This uses the memory mapped registers to send and recieve a command from the
// codec. These are not recommended for use by the manual and are really only
// there for the BIOS. Also this method can only do 32-bit responses instead of
// the full 64-bit responses that may be returned by some verbs.
//
// Parameters:
//   verb - The verb/command to send
//
// Returns:
//   The response from the codec
//=============================================================================
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

//=============================================================================
// hda_enable_interrupts
//
// Enable global and codec interrupts as well as RIRB interrupts. Does not
// enable stream interrupts.
//=============================================================================
static void hda_enable_interrupts()
{
#define GIE 0x80000000
#define CIE 0x40000000
	hda_ptr_32[INTCTL] |= GIE | CIE;

	hda_ptr_8[RIRBCTL] |= 0x1;
}

//=============================================================================
// read_audio_widget
//
// Helper method to read a bunch of information about an AudioFunctionGroup
// widget.
//
// Parameters:
//   afg - A pointer to the AudioFunctionGroup structure
//   widget - The node ID of the widget to interrogate
//=============================================================================
static AudioWidget* read_audio_widget(AudioFunctionGroup* afg, uint32_t widget)
{
	uint32_t verb = create_verb(afg->codec, 0, widget, 0xF00, 9);
	RIRB_Response resp = poll_command(verb);	
	uint32_t info = resp.response;

	kprintf("\nAudio Widget CAP: %d - 0x%x - 0x%x\n", widget, 
			resp.response, resp.resp_ex);

	AudioWidget* aw = (AudioWidget*)hda_alloc(sizeof(AudioWidget));

	// Set the widget's node
	aw->node = widget;

	// Read the capabilities
	aw->type               = (info >> 20) & 0xF;
	aw->delay              = (info >> 16) & 0xF;
	aw->channel_count      = ((((info >> 13) & 0x7) << 1) | (info & 0x1)) + 1;
	aw->cp_caps            = (info >> 12) & 0x1;
	aw->lr_swap            = (info >> 11) & 0x1;
	aw->power_control      = (info >> 10) & 0x1;
	aw->digital            = (info >> 9) & 0x1;
	aw->conn_list          = (info >> 8) & 0x1;
	aw->unsolicit_cap      = (info >> 7) & 0x1;
	aw->processing_widget  = (info >> 6) & 0x1;
	aw->stripe             = (info >> 5) & 0x1;
	aw->format_override    = (info >> 4) & 0x1;
	aw->amp_param_override = (info >> 3) & 0x1;
	aw->out_amp_present    = (info >> 2) & 0x1;
	aw->in_amp_present     = (info >> 1) & 0x1;

	const char* types[16] = {
		"Audio Output", "Audio Input", "Audio Mixer", "Audio Selector",
		"Pin Complex", "Power Widget", "Volume Knob Widget", "Beep Generator",
		"Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
		"Reserved", "Vendor Defined",
	};

	kprintf("Type: %s\n", types[aw->type]);

	// Read the pin capabilities
	verb = create_verb(afg->codec, 0, widget, 0xF00, 0xC);
	resp = poll_command(verb);
	info = resp.response;
	kprintf("Audio Widget PCAP: %d - 0x%x - 0x%x\n", widget, 
			resp.response, resp.resp_ex);

	aw->pin_caps.high_bit_rate       = (info >> 27) & 0x1;
	aw->pin_caps.display_port        = (info >> 24) & 0x1;
	aw->pin_caps.eapd_capable        = (info >> 16) & 0x1;
	aw->pin_caps.vref_control        = (info >> 8) & 0xFF;
	aw->pin_caps.hdmi                = (info >> 7) & 0x1;
	aw->pin_caps.balanced_io         = (info >> 6) & 0x1;
	aw->pin_caps.input_capable       = (info >> 5) & 0x1;
	aw->pin_caps.output_capable      = (info >> 4) & 0x1;
	aw->pin_caps.headphone_drive_cap = (info >> 3) & 0x1;
	aw->pin_caps.presence_detectable = (info >> 2) & 0x1;
	aw->pin_caps.trigger_required    = (info >> 1) & 0x1;
	aw->pin_caps.imped_sense_capable = info & 0x1;

	// Get the input amplifier capabilities of this node
	verb = create_verb(afg->codec, 0, widget, 0xF00, 0xD);	
	resp = poll_command(verb);
	info = resp.response;

	kprintf("Audio Widget ICAP: %d - 0x%x - 0x%x\n", widget, 
			resp.response, resp.resp_ex);

	aw->in_amp_caps.mute_capable = (info >> 31) & 0x1;
	aw->in_amp_caps.step_size    = (info >> 16) & 0x7F;
	aw->in_amp_caps.num_steps    = (info >> 8) & 0x7F;
	aw->in_amp_caps.offset       = info & 0x3F;

	// Get the output amplifier capabilities of this node
	verb = create_verb(afg->codec, 0, widget, 0xF00, 0x12);
	resp = poll_command(verb);
	info = resp.response;

	kprintf("Audio Widget OCAP: %d - 0x%x - 0x%x\n", widget, 
			resp.response, resp.resp_ex);

	aw->out_amp_caps.mute_capable = (info >> 31) & 0x1;
	aw->out_amp_caps.step_size    = (info >> 16) & 0x7F;
	aw->out_amp_caps.num_steps    = (info >> 8) & 0x7F;
	aw->out_amp_caps.offset       = info & 0x3F;

	// Get the connection list length
	verb = create_verb(afg->codec, 0, widget, 0xF00, 0xE);
	resp = poll_command(verb);
	info = resp.response;

	kprintf("Audio Widget CLST: %d - 0x%x - 0x%x\n", widget, 
			resp.response, resp.resp_ex);

	aw->conn_info.long_form = (info >> 7) & 0x1;
	aw->conn_info.length    = info & 0x7F;

	list_init(&aw->conn_info.lst_conn, hda_alloc, hda_free);

	if (aw->conn_info.length > 0)
	{
		kprintf("Conn List:\n");
		// Get all of the connections
		for (uint32_t n = 0; n < aw->conn_info.length; )
		{
			verb = create_verb(afg->codec, 0, widget, 0xF02, n);
			resp = poll_command(verb);
			info = resp.response;

			uint32_t times = 0;
			uint32_t mask = 0;
			uint32_t shift = 0;

			if (aw->conn_info.long_form)
			{
				times = 2;
				mask = 0xFFFF;
				shift = 16;
			}
			else
			{
				times = 4;
				mask = 0xFF;
				shift = 8;
			}

			for (uint32_t i = 0; i < times && n < aw->conn_info.length; ++i)
			{
				// We don't need to allocate space for the integer because it fits
				// inside the size of the pointer to the data
				const uint64_t data = info & mask;
				kprintf(" %u\n", data);
				list_insert_next(&aw->conn_info.lst_conn, NULL, (void*)data);
				info >>= shift;
				++n;
			}
		}
	}

	// Get the volume knob capabilities
	verb = create_verb(afg->codec, 0, widget, 0xF00, 0x13);
	resp = poll_command(verb);
	info = resp.response;

	kprintf("Audio Widget VOLK: %d - 0x%x - 0x%x\n", widget, 
			resp.response, resp.resp_ex);

	aw->volume_knob.delta     = (info >> 7) & 0x1;
	aw->volume_knob.num_steps = info & 0x7F;

	// Read the default configuration
	verb = create_verb(afg->codec, 0, widget, 0xF1C, 0);
	resp = poll_command(verb);
	info = resp.response;

	kprintf("Audio Widget DEFC: %d - 0x%x - 0x%x\n", widget, 
			resp.response, resp.resp_ex);

	aw->default_config.port_connectivity = (info >> 30) & 0x3;
	aw->default_config.location          = (info >> 24) & 0x3F;
	aw->default_config.default_device    = (info >> 20) & 0xF;
	aw->default_config.connection_type   = (info >> 16) & 0xF;
	aw->default_config.color             = (info >> 12) & 0xF;
	aw->default_config.misc              = (info >> 8) & 0xF;
	aw->default_config.default_assoc     = (info >> 4) & 0xF;
	aw->default_config.sequence          = info & 0xF;

	// Print some info about the default device
	const char* device[16] =
	{
		"Line Out", "Speaker", "HP Out", "CD",
		"SPDIF Out", "Digital Other Out", "Modem Line Side",
		"Modem Handset Side", "Line In", "AUX", "Mic In",
		"Telephony", "SPDIF In", "Digital Other In", "Reserved",
		"Other",
	};

	kprintf("Device Type: %s\n", device[aw->default_config.default_device]);

	return aw;
}

//=============================================================================
// find_widget
//
// Loops through an AudioFunctionGroup and tries to find a widget with the
// given node ID.
//
// Parameters:
//   afg - The AudioFunctionGroup structure
//   widget - The node ID of the widget
//
// Returns:
//   An AudioWidget* or NULL if there is no widget with the given node ID
//=============================================================================
static AudioWidget* find_widget(AudioFunctionGroup* afg, uint32_t widget)
{
	for (uint8_t i = 0; i < afg->nodes.length; ++i)
	{
		AudioWidget* aw = afg->nodes.widgets[i];
		if (aw->node == widget)
		{
			return aw;
		}
	}

	return NULL;
}

//=============================================================================
// hda_dfs
//
// This method is a very simple quick way of finding a path from a pin complex
// to an audio converter. Right now it really only works for audio output 
// converters.
//
// Parameters:
//   path - The list that will store all of the nodes in the path
//   afg - The AudioFunctionGroup structure
//   aw - The current AudioWidget
//
// Returns:
//   1 if successfully found a path from a port to a converter, 0 otherwise
//=============================================================================
static uint64_t hda_dfs(linked_list_t* path, AudioFunctionGroup* afg, AudioWidget* aw)
{
	if (aw->type == AW_TYPE_OUT_CONV)// ||
		//aw->type == AW_TYPE_IN_CONV)
	{
		// We're done we found the end!
		list_insert_next(path, NULL, aw);
		return 1;
	}
	else if (aw->conn_info.length > 0)
	{
		// We did not find it, so search our connections
		list_element_t* node = list_head(&aw->conn_info.lst_conn);
		while (node != NULL)
		{
			const uint64_t widget_num = (uint64_t)list_data(node);

			// Find it in the AFG list
			AudioWidget* aw_next = find_widget(afg, widget_num);
			ASSERT(aw_next != NULL);

			// Recursive call
			if (hda_dfs(path, afg, aw_next))
			{
				// We found the solution, add ourselves to the path
				list_insert_next(path, NULL, aw);
				return 1;
			}

			// We did not find the solution, onto the next connection
			node = list_next(node);
		}
	}

	return 0;
}

//=============================================================================
// hda_setup_connections
//
// This method is meant to find or create connections from input/output ports
// to audio converters. Right now it only really does anything useful with
// audio output converters.
//
// Parameters:
//   afg - The AudioFunctionGroup structure
//=============================================================================
static void hda_setup_connections(AudioFunctionGroup* afg)
{
	// The AFG passed in has had all of its widgets read
	// and now we need to figure out which ones are the
	// ones we care about and how to use them to achieve
	// what we want to do.
	
	// Start at the pin complexes and look for audio output
	// converters
	for (uint8_t i = 0; i < afg->nodes.length; ++i)
	{
		kprintf("Num widgets: %u\n", afg->nodes.length);
		AudioWidget* aw = afg->nodes.widgets[i];
		kprintf("Trying: %u\n", aw->node);
		if (aw->type == AW_TYPE_PIN_COMPLEX)// &&
				// Line out
				//aw->default_config.default_device == 0x0)
		{
			// Work our way backwards to an audio input converter
			// or an audio output converter, BFS	
			linked_list_t path;
			list_init(&path, hda_alloc, hda_free);

			if (hda_dfs(&path, afg, aw))
			{
				// Print the path
				list_element_t* node = list_head(&path);

				kprintf("Path\n");
				while (node != NULL)
				{
					AudioWidget* aw = (AudioWidget*)list_data(node);
					kprintf(" %u\n", aw->node);
					node = list_next(node);
				}

				AudioOutputPath* aop = (AudioOutputPath*)hda_alloc(sizeof(AudioOutputPath));
				aop->path = path;
				list_insert_next(&afg->lst_outputs, NULL, aop);
			}
		}
	}

	// Make sure we found some paths
}

//=============================================================================
// hda_enumerate_codecs
//
// This method enumerates all of the available codecs looking for an 
// Audio Function Group (AFG) codec.
//=============================================================================
static void hda_enumerate_codecs()
{
	kprintf("DMA LOC: 0x%x\n", hda_dma_phys_loc);

	// Write positions of the DMA engines periodically
	hda_ptr_32[DPLBASE] = (hda_dma_phys_loc & 0xFFFFFFFF) | 0x1;
	hda_ptr_32[DPUBASE] = (hda_dma_phys_loc >> 32) & 0xFFFFFFFF;

	hda_enable_corb_dma();
	hda_enable_rirb_dma();

	uint8_t cad = 0;
	for (int i = 0; i < 8; ++i, ++cad)
	{
		if (!(codecs_available & (1 << i)))
		{
			continue;
		}

		uint32_t verb = (cad << 28) | 0xF0000;
		RIRB_Response resp = poll_command(verb);
		kprintf("Response: 0x%x - 0x%x\n", 
				resp.response, resp.resp_ex);

		// Figure out how many nodes there are, parameter ID 0x4
		// Response is:
		//   31-24  |    23:16     |   15:8   |    7:0
		// Reserved | Start Node # | Reserved | Total # of Nodes
		verb = (cad << 28) | 0xF0004;
		resp = poll_command(verb);
		kprintf("Response: 0x%x - 0x%x\n", 
				resp.response, resp.resp_ex);

		const uint8_t total_nodes = resp.response & 0xFF;
		uint8_t start_node = (resp.response >> 16) & 0xFF;

		kprintf("Sub nodes: %u\n", total_nodes);

		for (uint32_t i = 0; i < total_nodes; ++i, ++start_node)
		{
			verb = create_verb(cad, 0, start_node, 0xF00, 5);
			resp = poll_command(verb);
			kprintf("Function Type: 0x%x - 0x%x\n", 
					resp.response, resp.resp_ex);
			if (resp.response == 0x1) // AFG
			{
				hda_afg.codec = cad;
				hda_afg.node = start_node;
				list_init(&hda_afg.lst_outputs, hda_alloc, hda_free);
				// Get the capabilities
				verb = create_verb(cad, 0, start_node, 0xF00, 8);
				resp = poll_command(verb);
				kprintf("AFG Caps: 0x%x - 0x%x\n", 
						resp.response, resp.resp_ex);
				// Get the 'widgets'
				verb = create_verb(cad, 0, start_node, 0xF00, 4);
				resp = poll_command(verb);
				kprintf("Sub nodes: 0x%x - 0x%x\n", 
						resp.response, resp.resp_ex);
				uint8_t widget_total = resp.response & 0xFF;
				uint8_t widget_start = (resp.response >> 16) & 0xFF;
				hda_afg.nodes.base = widget_start;
				hda_afg.nodes.length = widget_total;
				hda_afg.nodes.widgets = 
					(AudioWidget**)hda_alloc(hda_afg.nodes.length*sizeof(AudioWidget*));
				kprintf("AFG sub nodes: %u\n", widget_total);
				for (int j = 0; j < widget_total; ++j, ++widget_start)
				{
					AudioWidget* aw = read_audio_widget(&hda_afg, widget_start);
					hda_afg.nodes.widgets[j] = aw;
				}
				//break;
				cad = 8;
				break;
			}
		}
	}

	// Now we need to figure out how to get sound from the buffers
	// to the speakers
	hda_setup_connections(&hda_afg);
}	

//=============================================================================
// hda_interrupt_handler
//
// The interrupt handler for the HDA driver
//=============================================================================
static void hda_interrupt_handler(uint64_t vector, uint64_t error)
{
	UNUSED(vector);
	UNUSED(error);

	kprintf("GOT HDA INTERRUPT!\n");
	__asm__ volatile("hlt");

	//pic_acknowledge(vector);
}

//=============================================================================
// hda_setup_streams
//
// Creates a number of in memory buffers that can be used for transporting
// sound and stream commands between the codec and OS.
//=============================================================================
static void hda_setup_streams()
{
	// Read the GCAP register to see how many streams this device supports
	const uint16_t gcap = hda_ptr_16[GCAP];

	hda_num_output_streams = (gcap >> 12) & 0xF;
	hda_num_input_streams = (gcap >> 8)  & 0xF;

	hda_stream_regs = (volatile StreamReg*)(HDA_MEM_LOC + SDCTL0);

	ASSERT(hda_num_output_streams >= 1);

	kprintf("Supports: %u output streams %u input streams\n", 
			hda_num_output_streams, hda_num_input_streams);

	const uint8_t bdl_64_bit = gcap & 0x1;
	kprintf("Supports 64-bit addressing: %u\n", bdl_64_bit);

	// Setup the data buffers for the streams
	// The link rate is based off of a 48Khz frame time	
	// buffers must start on a 128 byte boundary	
	// needs to be able to contain at least one full packet
	// length should be a multiple of 128 bytes
	// use BDLs to inform the hardware

	list_init(&hda_lst_streams, hda_alloc, hda_free);

	uint64_t stream_bdl_addr = HDA_STREAM_BASE;
	uint64_t stream_data_addr = HDA_STREAM_DATA_BASE;

	// Focus on output streams for now
	for (uint16_t i = 0; i < hda_num_output_streams; ++i)
	{
		uint64_t phys_loc = 0;
		virt_map_page(kernel_table, stream_data_addr,
				PG_FLAG_RW | PG_FLAG_PCD | PG_FLAG_PWT, PAGE_LARGE, &phys_loc);
		stream_data_addr += PAGE_LARGE_SIZE;

		Stream* stream = hda_alloc(sizeof(Stream));

		stream->number = i;

		stream->bdl_info.data_address = stream_data_addr;
		stream->bdl_info.data_phys_address = phys_loc;
		stream->bdl_info.data_length = PAGE_LARGE_SIZE;

		virt_map_page(kernel_table, stream_bdl_addr,
				PG_FLAG_RW | PG_FLAG_PCD | PG_FLAG_PWT, PAGE_SMALL, &phys_loc);
		stream_bdl_addr += PAGE_SMALL_SIZE;

		stream->bdl_info.bdl_buffer_address = stream_bdl_addr;
		stream->bdl_info.bdl_buffer_phys_address = phys_loc;
		stream->bdl_info.bdl_buffer_length = PAGE_SMALL_SIZE;

		list_insert_next(&hda_lst_streams, NULL, stream);
	}
}

#define STREAM_CTL_DEIE 0x10 // Descriptor error interrupt enable
#define STREAM_CTL_FEIE 0x08 // FIFO error interrupt enable
#define STREAM_CTL_IOCE 0x04 // Interrupt on completion enable
#define STREAM_CTL_SRUN 0x02 // Stream run
#define STREAM_CTL_SRST 0x01 // Stream reset
static uint16_t stream_get_format(const Stream* s)
{
	return ((s->descriptor.sample_base_rate & 0x1) << 14) |
		   ((s->descriptor.sample_base_rate_multiple & 0x7) << 11) |
		   ((s->descriptor.sample_base_rate_divisor & 0x7) << 8) |
		   ((s->descriptor.bits_per_sample & 0x7) << 4) |
		   (s->descriptor.number_of_channels & 0xF);
}

static volatile StreamReg* stream_get_sreg(const Stream* s, uint8_t output)
{
	volatile StreamReg* s_reg = output ? 
		&hda_stream_regs[s->number+hda_num_input_streams] :
		&hda_stream_regs[s->number];
	return s_reg;
}

static void stream_enable(const Stream* s, uint8_t output)
{
	volatile StreamReg* s_reg = stream_get_sreg(s, output);

	// Make sure the stream is not running
	s_reg->SDCTL_STS.bytes[0] |= 0x2;
	while (!(s_reg->SDCTL_STS.bytes[0] & 0x2));
}

static void stream_disable(const Stream* s, uint8_t output)
{
	volatile StreamReg* s_reg = stream_get_sreg(s, output);

	// Make sure the stream is not running
	s_reg->SDCTL_STS.bytes[0] &= ~0x2;
	while (s_reg->SDCTL_STS.bytes[0] & 0x2);
}

static void configure_stream(const Stream* s, uint8_t output)
{
	volatile StreamReg* s_reg = stream_get_sreg(s, output);

	// Set the stream number
	s_reg->SDCTL_STS.bytes[2] &= 0xF;
	s_reg->SDCTL_STS.bytes[2] |= ((s->number+1) & 0xF) << 4;
	s_reg->SDCTL_STS.bytes[0] |= 1;
	kprintf("\n\n\nSREG: 0x%x\n", s_reg);
	uint32_t i = 0;
	for (; i < 8; ++i)
	{
		kprintf("REG: 0x%x\n", hda_ptr_32[((0x80+i*0x20)/4)]);
	}

	// Make sure the stream is not running
	__asm__ volatile("cli");
	stream_disable(s, output);
	__asm__ volatile("cli");

	// Reset the stream
	s_reg->SDCTL_STS.all |= 0x1;
	kprintf("Status1: 0x%x\n", s_reg->SDCTL_STS.all);
	while (!(s_reg->SDCTL_STS.all & 0x1));
	kprintf("Status2: 0x%x\n", s_reg->SDCTL_STS.all);
	s_reg->SDCTL_STS.all &= ~0x1;
	while (s_reg->SDCTL_STS.all & 0x1);
	__asm__ volatile("cli");
	//__asm__ volatile("hlt");

	// Set the stream number
	s_reg->SDCTL_STS.bytes[2] &= 0xF;
	s_reg->SDCTL_STS.bytes[2] |= ((s->number+1) & 0xF) << 4;

	// Set high priority traffic
	s_reg->SDCTL_STS.bytes[2] |= 0x4; 

	// Setup the buffer descriptor registers
	s_reg->SDCBL = s->bdl_info.bdl_buffer_length;

	// Set the format register
	s_reg->SDFMT = stream_get_format(s);

	// Set the buffer descriptor list pointer registers
	s_reg->SDBDP = s->bdl_info.bdl_buffer_phys_address;
}


//=============================================================================
// hda_test_sound 
//
// Plays a sample sound file
//=============================================================================
static void hda_test_sound()
{
	ASSERT(list_size(&hda_afg.lst_outputs) > 0);

	kprintf("Paths: %u\n", list_size(&hda_afg.lst_outputs));
	// Go through the output chain and set everything up
	AudioOutputPath* aop = (AudioOutputPath*)list_data(list_next(list_head(&hda_afg.lst_outputs)));
	ASSERT(list_size(&aop->path) > 0);

	list_element_t* node = list_head(&aop->path);
	while (node != NULL)
	{
		AudioWidget* aw = (AudioWidget*)list_data(node);

		// Set the output amplifier parameters
		if (aw->out_amp_present)
		{
			uint32_t verb = create_verb(hda_afg.codec, 0, aw->node,
					0xB00, (1 << 15) | (1 << 13));
			RIRB_Response resp = poll_command(verb);
			kprintf("Getting amp: 0x%x\n", resp.response);

			kprintf("Setting amp: %u\n", aw->node);
			uint16_t data = (0x1 << 15)	| (0x3 << 12) | 0xA;
			//	((aw->out_amp_caps.num_steps-1)*aw->out_amp_caps.step_size);
			kprintf("Steps: %u - Size: %u\n", aw->out_amp_caps.num_steps,
					aw->out_amp_caps.step_size);
			verb = create_verb(hda_afg.codec, 0, aw->node, 0x300, data);
			resp = poll_command(verb);
			//kprintf("Set: 0x%x\n", resp.response);

			verb = create_verb(hda_afg.codec, 0, aw->node,
					0xB00, (1 << 15) | (1 << 13));
			resp = poll_command(verb);
			kprintf("After Set: 0x%x\n", resp.response);
		}

		// Check what type of widget this is
		if (aw->type == AW_TYPE_PIN_COMPLEX)
		{
			kprintf("Test Pin Complex\n");
			// Make sure it supports output
			ASSERT(aw->pin_caps.output_capable);

			// Enable the output	
			uint32_t verb = create_verb(hda_afg.codec, 0, aw->node, 0xF07, 0);
			RIRB_Response resp = poll_command(verb);
			kprintf("Response: 0x%x\n", resp.response);
			// Enable the output
			resp.response |= 0x40;
			verb = create_verb(hda_afg.codec, 0, aw->node, 0x707, resp.response & 0xFF);
			resp = poll_command(verb);
		}
		else if (aw->type == AW_TYPE_OUT_CONV)
		{
			// Setup the stream, stream 0 is by convention reserved...
			ASSERT(list_size(&hda_lst_streams) > 0);

			Stream* stream = (Stream*)list_data(list_tail(&hda_lst_streams));
			kprintf("Setting output converter - Stream %u\n", stream->number);
			uint32_t verb = create_verb(hda_afg.codec, 0, aw->node, 0xF06, 0);
			RIRB_Response resp = poll_command(verb);
			//kprintf("OUTPUT: 0x%x\n", resp.response);
			
			// Set the converter channel count = 1
			verb = create_verb(hda_afg.codec, 0, aw->node, 0x72D, 0);
			poll_command(verb);

			uint8_t data = ((stream->number+1) << 4) | 0x0; // Stream 1, channel 0 = lowest
			verb = create_verb(hda_afg.codec, 0, aw->node, 0x706, data);
			poll_command(verb);

			// Setup the stream's parameters
			stream->descriptor.sample_base_rate = 0;
			stream->descriptor.sample_base_rate_multiple = 0;
			stream->descriptor.sample_base_rate_divisor = 0x3;
			stream->descriptor.bits_per_sample = 8;
			stream->descriptor.number_of_channels = 0;
			configure_stream(stream, 1);

			// Set the converter format
			uint16_t stream_format = stream_get_format(stream);
			verb = create_verb(hda_afg.codec, 0, aw->node, 0x200, stream_format);
			poll_command(verb);

			// Set the streams BDL
			BDL_Entry* bdl = (BDL_Entry*)stream->bdl_info.bdl_buffer_address;
			bdl[0].address = stream->bdl_info.data_phys_address;
			bdl[0].length = small_wav_len;

			volatile StreamReg* s_reg = stream_get_sreg(stream, 1);
			s_reg->SDLVI = 1;

			uint8_t* music = (uint8_t*)stream->bdl_info.data_address;
			uint32_t m_i = 0;
			for (uint32_t i = 0; i < small_wav_len; ++i)
			{
				music[m_i++] = 0;
				music[m_i++] = small_wav[i];
			}

			// Need at least two entries
			bdl[1] = bdl[0];
		}

		node = list_next(node);
	}

	// Enable the stream
	Stream* stream = (Stream*)list_data(list_tail(&hda_lst_streams));
	kprintf("Stream SDCTL: 0x%x\n", stream_get_sreg(stream, 1)->SDCTL_STS.all);
	volatile StreamReg* s_reg = stream_get_sreg(stream, 1);
	stream_enable(stream, 1);
	kprintf("Stream SDCTL2: 0x%x\n", s_reg->SDCTL_STS.all);
}

//=============================================================================
// find_custom_first_pass
//
// This method tries to find PCI devices that are compatible with this HDA
// driver. This method checks specific vendor and device id's. This is called
// by the pci_find_custom() method.
//=============================================================================
static uint8_t find_custom_first_pass(const pci_config_t* config)
{
	return config->vendor_id == 0x8086
		&& (config->device_id == 0x2668
				|| config->device_id == 0x284B
				|| config->device_id == 0x1C20);
}

//=============================================================================
// find_custom_second_pass
//
// This method uses a more generic way to find a PCI device that may be
// compatible with this driver. It looks for a specific base class and sub
// class that seem to appear on HDA hardware. This method is a backup to the
// first one.
//=============================================================================
static uint8_t find_custom_second_pass(const pci_config_t* config)
{
	return config->vendor_id == 0x8086
		&& config->base_class == 0x4
		&& config->sub_class == 0x3;
}

//=============================================================================
// sound_init
//
// The starting point for the initialization of the HDA sound driver. It 
// performs mapping of the PCI configuration space and calls all of the other
// methods for setting up buffers and registers.
//=============================================================================
void sound_init()
{
	kprintf("Initializing sound\n");
	pci_init();

	// Try to find the correct PCI device
	pci_hda_config = pci_find_custom(find_custom_first_pass);
	if (pci_hda_config == NULL)
	{
		pci_hda_config = pci_find_custom(find_custom_second_pass);
		if (pci_hda_config == NULL)
		{
			panic("Failed to find HDA device");
		}
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
			PG_FLAG_RW | PG_FLAG_PCD | PG_FLAG_PWT, PAGE_SMALL, 4);
	virt_map_page(kernel_table, HDA_RING_LOC,
			PG_FLAG_RW | PG_FLAG_PCD, PAGE_SMALL, &hda_ring_phys_loc);
	virt_map_page(kernel_table, HDA_DMA_LOC,
			PG_FLAG_RW | PG_FLAG_PCD | PG_FLAG_PWT, PAGE_SMALL, &hda_dma_phys_loc);
	memclr((void*)HDA_DMA_LOC, PAGE_SMALL_SIZE);
	memclr((void*)HDA_RING_LOC, PAGE_SMALL_SIZE);

	// Enable memory writes
	uint32_t command = pci_config_read_w(pci_hda_config, PCI_COMMAND);
	command |= PCI_CMD_MEMORY | PCI_CMD_MASTER | 0x20;
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

	// Setup the streams
	hda_setup_streams();

	// Figure out what we have
	hda_enumerate_codecs();

	kprintf("Playing sound...\n");

	// Try sound
	hda_test_sound();
}
