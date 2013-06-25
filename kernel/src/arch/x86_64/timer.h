#ifndef __X86_64_TIMER_H__
#define __X86_64_TIMER_H__

#include "inttypes.h"

#define CLOCK_FREQUENCY 1000

void timer_init(void);

void timer_one_shot(void);

void timer_set_delay(const uint64_t delay);

#endif
