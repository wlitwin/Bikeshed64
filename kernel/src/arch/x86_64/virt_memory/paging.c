#include "paging.h"
#include "physical.h"

#include "arch/x86_64/serial.h"
#include "arch/x86_64/textmode.h"

/* How many virtual address bits the processor has
 *
 * Defined in prekernel.s
 */
extern uint8_t processor_virt_bits;

void virt_memory_init()
{
	/* XXX - Temporarily here for debugging */
	//init_serial_debug();	
	init_text_mode();

	phys_memory_init();
}
