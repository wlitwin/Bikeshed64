#ifndef __KERNEL_TIMER_TIMER_H__
#define __KERNEL_TIMER_TIMER_H__

#ifdef BIKESHED_X86_64
#include "arch/x86_64/timer.h"
#endif

extern void timer_init(void);

extern void timer_one_shot(void);

extern void timer_set_delay(const uint64_t delay);

#endif
