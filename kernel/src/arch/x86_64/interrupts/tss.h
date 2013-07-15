#ifndef __X86_64_INTERRUPTS_TSS_H__
#define __X86_64_INTERRUPTS_TSS_H__

#include "defines.h"
#include "inttypes.h"

void setup_tss_descriptor(void);

void tss_set_context_stack(const uint64_t location);

#endif
