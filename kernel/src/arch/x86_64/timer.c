#include "timer.h"

#include "safety.h"
#include "support.h"
#include "kprintf.h"
#include "interrupts/interrupts.h"

#define TIMER_FREQ 1193182 /* Megahertz */

#define CMD_PORT 0x43
#define CH0_DATA_PORT 0x40
#define CMD_ONE_SHOT (0x4 << 1)
#define CMD_LO_HI_BYTE (0x3 << 4)
#define CMD_SEL_CH0 0x0

//extern inline void timer_interrupt(void);
void timer_isr(uint64_t vector, uint64_t error_code);

void timer_init()
{
	_outb(CMD_PORT, CMD_ONE_SHOT | CMD_LO_HI_BYTE | CMD_SEL_CH0);	

	interrupts_install_isr(VEC_TIMER, &timer_isr);

	timer_one_shot(0);
}

void timer_one_shot(uint16_t time)
{
	__asm__ volatile ("pushfq");
	__asm__ volatile ("cli");

	const uint8_t lo_byte = time & 0xFFFF;
	const uint8_t hi_byte = (time >> 16) & 0xFFFF;

	_outb(CH0_DATA_PORT, lo_byte);
	_outb(CH0_DATA_PORT, hi_byte);

	__asm__ volatile ("popfq");
}

void timer_isr(uint64_t vector, uint64_t error_code)
{
	UNUSED(error_code);
//	timer_interrupt();

	kprintf("Timer!");

	pic_acknowledge(vector);
}
