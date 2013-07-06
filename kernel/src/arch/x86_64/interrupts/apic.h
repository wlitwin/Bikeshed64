#ifndef __X86_64_INTERRUPTS_APIC_H__
#define __X86_64_INTERRUPTS_APIC_H__

#include "inttypes.h"

typedef uint32_t time_t;

/* Initializes the local APIC. Currently this feature of the AMD64
 * architecture is not supported. This initialization function simply
 * checks for its existence and then disables it. In the future 
 * this will be used but for simplicity and time constraints the old
 * style PIC is being used.
 */
void apic_init(void);

void apic_eoi(void);

void timer_set_delay(uint32_t delay);

uint32_t timer_get_count(void);

uint32_t timer_one_ms(void);

void timer_resume(void);

void timer_start(void);

void timer_stop(void);

#endif
