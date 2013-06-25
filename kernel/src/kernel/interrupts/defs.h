#ifndef __INTERRUPT_DEFS_H__
#define __INTERRUPT_DEFS_H__

#include "inttypes.h"

typedef void (*interrupt_handler)(uint64_t vector, uint64_t error_code);

/* Initialize the interrupt sub-system. This function is defined in
 * the architecture specific files.
 */
extern void interrupts_init(void);

/* Install an interrupt handler at the specified vector number.
 */
extern void interrupts_install_isr(uint64_t vector, interrupt_handler handler);

#ifdef BIKESHED_X86_64
#include "arch/x86_64/interrupts/interrupts.h"
#endif

#endif
