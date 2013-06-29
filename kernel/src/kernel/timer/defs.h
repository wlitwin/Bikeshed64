#ifndef __KERNEL_TIMER_TIMER_H__
#define __KERNEL_TIMER_TIMER_H__

#ifdef BIKESHED_X86_64
#include "arch/x86_64/interrupts/apic.h"
#endif

void timer_set_delay(uint32_t delay);

uint32_t timer_get_count(void);

uint32_t timer_one_ms(void);

uint32_t timer_get_elapsed(void);

void timer_resume(void);

void timer_start(void);

void timer_stop(void);

#endif
