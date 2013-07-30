#include "hda.h"
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
//=============================================================================
static void hda_disable_corb_dma()
{
	hda_ptr_8[CORBCTL] &= ~CORB_DMA_RUN;
	while (hda_ptr_8[CORBCTL] & CORB_DMA_RUN);
}

//=============================================================================
//=============================================================================
static void hda_enable_corb_dma()
{
	hda_ptr_8[CORBCTL] |= CORB_DMA_RUN;
}

//=============================================================================
//=============================================================================
static void hda_disable_rirb_dma()
{
	hda_ptr_8[RIRBCTL] &= ~RIRB_DMA_RUN;
	while (hda_ptr_8[RIRBCTL] & RIRB_DMA_RUN);
}

//=============================================================================
//=============================================================================
static void hda_enable_rirb_dma()
{
	hda_ptr_8[RIRBCTL] |= RIRB_DMA_RUN;
}

//=============================================================================
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

/* Figure out how much space is left in a ring buffer.
 * Assumes that the readptr is always behind the writeptr.
 *
 * Returns:
 *    How much space is left in the buffer based on the 
 *    total buffer size, readptr and writeptr.
 */
//=============================================================================
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


/* Check if the RIRB has any responses waiting to be read
 *
 * Returns:
 *    1 - if a response is available, 0 otherwise
 */
//=============================================================================
//=============================================================================
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
//=============================================================================
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

/* Sends a command to the corb. If there is no space available it will sit in
 * a busy loop until space becomes available.
 *
 * Side Effects:
 *    Increments the write pointer.
 *    Stores command in CORB.
 */
//=============================================================================
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
//=============================================================================
static RIRB_Response poll_command(uint32_t verb)
{
	corb_send_command(verb);
	while (!rirb_has_responses());
	return rirb_read_response();
}

//=============================================================================
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
//=============================================================================
static void hda_enable_interrupts()
{
#define GIE 0x80000000
#define CIE 0x40000000
	hda_ptr_32[INTCTL] |= GIE | CIE;

	hda_ptr_8[RIRBCTL] |= 0x1;
}

//=============================================================================
//=============================================================================
static void read_audio_widget(AudioFunctionGroup* afg, uint32_t widget)
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
//#define AW_TYPE_PIN 0x4
//#define AW_TYPE_IN_AMP 0x1
//#define AW_TYPE_OUT_AMP 0x0

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

	// Add this widget to this function groups list
	list_insert_next(&afg->lst_widgets, NULL, aw);
}

//=============================================================================
//=============================================================================
static AudioWidget* find_widget(AudioFunctionGroup* afg, uint32_t widget)
{
	list_element_t* node = list_head(&afg->lst_widgets);
	while (node != NULL)
	{
		AudioWidget* aw = (AudioWidget*)list_data(node);

		if (aw->node == widget)
		{
			return aw;
		}

		node = list_next(node);
	}

	return NULL;
}

//=============================================================================
//=============================================================================
#define AW_TYPE_OUT_CONV 0x0
#define AW_TYPE_IN_CONV 0x1
#define AW_TYPE_PIN_COMPLEX 0x4
static uint64_t hda_dfs(linked_list_t* path, AudioFunctionGroup* afg, AudioWidget* aw)
{
	if (aw->type == AW_TYPE_OUT_CONV ||
		aw->type == AW_TYPE_IN_CONV)
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
//=============================================================================
static void hda_setup_connections(AudioFunctionGroup* afg)
{
	// The AFG passed in has had all of its widgets read
	// and now we need to figure out which ones are the
	// ones we care about and how to use them to achieve
	// what we want to do.
	
	// Start at the pin complexes and look for audio output
	// converters
	list_element_t* node = list_head(&afg->lst_widgets);	
	kprintf("Num widgets: %u\n", list_size(&afg->lst_widgets));
	while (node != NULL)
	{
		AudioWidget* aw = (AudioWidget*)list_data(node);
		kprintf("Trying: %u\n", aw->node);
		if (aw->type == AW_TYPE_PIN_COMPLEX &&
			// Line out
			aw->default_config.default_device == 0x0)
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

		node = list_next(node);
	}
}

//=============================================================================
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
				list_init(&hda_afg.lst_widgets, hda_alloc, hda_free);
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
				kprintf("AFG sub nodes: %u\n", widget_total);
				for (int j = 0; j < widget_total; ++j, ++widget_start)
				{
					read_audio_widget(&hda_afg, widget_start);
				}
				//break;
				cad = 8;
			}
		}
	}

	// Now we need to figure out how to get sound from the buffers
	// to the speakers
	hda_setup_connections(&hda_afg);
}	

//=============================================================================
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
//=============================================================================
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
	// The link rate is based off of a 48Khz frame time	
	// buffers must start on a 128 byte boundary	
	// needs to be able to contain at least one full packet
	// length should be a multiple of 128 bytes
	// use BDLs to inform the hardware
	
	list_init(&hda_lst_streams, hda_alloc, hda_free);
	
	uint64_t stream_bdl_addr = HDA_STREAM_BASE;
	uint64_t stream_data_addr = HDA_STREAM_DATA_BASE;

	// Focus on output streams for now
	for (uint16_t i = 0; i < num_output_streams; ++i)
	{
		uint64_t phys_loc = 0;
		virt_map_page(kernel_table, stream_data_addr,
				PG_FLAG_RW | PG_FLAG_PCD | PG_FLAG_PWT, PAGE_LARGE, &phys_loc);
		stream_data_addr += PAGE_LARGE_SIZE;

		Stream* stream = hda_alloc(sizeof(Stream));

		stream->number = i;
		stream->in_sdctl_offset  = SDCTL0 + i;
		stream->out_sdctl_offset = SDCTL0 + num_input_streams + i;
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

//=============================================================================
//=============================================================================
static uint8_t find_custom_first_pass(const pci_config_t* config)
{
	return config->vendor_id == 0x8086
		&& (config->device_id == 0x2668
			 || config->device_id == 0x284B
			 || config->device_id == 0x1C20);
}

//=============================================================================
//=============================================================================
static uint8_t find_custom_second_pass(const pci_config_t* config)
{
	return config->vendor_id == 0x8086
		&& config->base_class == 0x4
		&& config->sub_class == 0x3;
}

//=============================================================================
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
}
