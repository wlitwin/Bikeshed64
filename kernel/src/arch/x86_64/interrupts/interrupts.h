#ifndef __X86_64_INTERRUPTS_INTERRUPTS_H__
#define __X86_64_INTERRUPTS_INTERRUPTS_H__

#include "inttypes.h"
#include "arch/x86_64/support.h"

void interrupts_init(void);

#define PIC_MASTER_CMD_PORT 0x20
#define PIC_MASTER_IMR_PORT 0x21
#define PIC_SLAVE_CMD_PORT 0xA0
#define PIC_SLAVE_IMR_PORT 0xA1
#define PIC_MASTER_SLAVE_LINE 0x04
#define PIC_SLAVE_ID 0x02
#define PIC_86MODE 0x1
#define PIC_ICW1BASE 0x10
#define PIC_NEEDICW4 0x01
#define PIC_EOI 0x20

static inline __attribute__((always_inline))
void pic_acknowledge(const uint64_t vector)
{
	if (vector >= 0x20 && vector < 0x30)
	{
		_outb(PIC_MASTER_CMD_PORT, PIC_EOI);
		if (vector > 0x28)
		{
			_outb(PIC_SLAVE_CMD_PORT, PIC_EOI);
		}
	}
}

#endif
