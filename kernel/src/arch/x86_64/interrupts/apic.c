#include "apic.h"
#include "interrupts.h"

#include "arch/x86_64/virt_memory/paging.h"

#include "arch/x86_64/cpuid.h"
#include "arch/x86_64/panic.h"
#include "arch/x86_64/kprintf.h"

#ifndef DEBUG_APIC
#define kprintf(...)
#endif

#define TIMER_FREQ 1193182 /* Megahertz */
#define CMD_PORT 0x43
#define CH0_DATA_PORT 0x40
#define CH2_DATA_PORT 0x42
#define CMD_ONE_SHOT (0x4 << 1)
#define CMD_LO_HI_BYTE (0x3 << 4)
#define CMD_SEL_CH0 0x0
#define CMD_HW_ONE_SHOT 0x2
#define CMD_SEL_CH2 0x80
#define PC_SPEAKER 0x61

#define APIC_BASE_MSR 0x1B
#define APIC_EOI (0xB0/sizeof(uint32_t))
#define APIC_VIRT_LOC 0xFFFFFFFFFFFFF000
#define APIC_VER_REG (0x30/sizeof(uint32_t))
#define APIC_TIMER_REG (0x320/sizeof(uint32_t))
#define APIC_CMCI_REG (0x2F0/sizeof(uint32_t))
#define APIC_LINT0_REG (0x350/sizeof(uint32_t))
#define APIC_LINT1_REG (0x360/sizeof(uint32_t))
#define APIC_ERROR_REG (0x370/sizeof(uint32_t))
#define APIC_PERF_CNT_REG (0x340/sizeof(uint32_t))
#define APIC_THERM_SEN_REG (0x330/sizeof(uint32_t))
#define APIC_LDR (0xD0/sizeof(uint32_t))
#define APIC_DFR (0xE0/sizeof(uint32_t))
#define APIC_TPR (0x80/sizeof(uint32_t))
#define APIC_SPURIOUS_REG (0xF0/sizeof(uint32_t))
#define APIC_TIMER_DIV_REG (0x3E0/sizeof(uint32_t))
#define APIC_TIMER_INIT_REG (0x380/sizeof(uint32_t))
#define APIC_TIMER_CUR_CNT (0x390/sizeof(uint32_t))

#define APIC_DISABLE (0x10000)
#define APIC_NMI (0x400)
#define APIC_SOFT_EN (0x100)

#define APIC_ESR (0x280/sizeof(uint32_t))

static uint8_t check_apic()
{
	// First check if we have a local APIC
	uint32_t eax, edx;
	cpuid(0x1, &eax, &edx);

	// local APIC == bit 9
	return (edx & 0x100) > 0;
}

static void enable_apic()
{
	uint32_t eax, edx;
	readmsr(APIC_BASE_MSR, &eax, &edx);
	eax |= 0x800;
	writemsr(APIC_BASE_MSR, eax, edx);
}

/*
static void disable_apic()
{
	uint32_t eax, edx;
	readmsr(APIC_BASE_MSR, &eax, &edx);
	// Turn off the local APIC by clearing bit 11
	eax &= ~(0x800);
	writemsr(APIC_BASE_MSR, eax, edx);
}
*/

static uint32_t apic_base_address()
{
	uint32_t eax, edx;
	readmsr(APIC_BASE_MSR, &eax, &edx);
	return eax & 0xFFFFF000;
}

/*
static uint8_t is_bootstrap_proc()
{
	uint32_t eax, edx;
	readmsr(APIC_BASE_MSR, &eax, &edx);
	return (eax & 0x100) > 0;
}
*/

/*
static uint8_t can_disable_apic()
{
	uint32_t eax, edx;
	readmsr(APIC_BASE_MSR, &eax, &edx);
	return eax & 0x800;
}
*/

static void pic_init()
{
	// ICW1
	_outb(PIC_MASTER_CMD_PORT, PIC_ICW1BASE | PIC_NEEDICW4);		
	_outb(PIC_SLAVE_CMD_PORT, PIC_ICW1BASE | PIC_NEEDICW4);

	// ICW2
	// Master offset to 32
	// Slave offset to 40
	_outb(PIC_MASTER_IMR_PORT, 0x20);
	_outb(PIC_SLAVE_IMR_PORT, 0x28);

	// ICW3
	_outb(PIC_MASTER_IMR_PORT, PIC_MASTER_SLAVE_LINE);
	_outb(PIC_SLAVE_IMR_PORT, PIC_SLAVE_ID);

	// ICW4
	_outb(PIC_MASTER_IMR_PORT, PIC_86MODE);
	_outb(PIC_SLAVE_IMR_PORT, PIC_86MODE);

	// OCW1 Allow interrupts on all lines
	_outb(PIC_MASTER_IMR_PORT, 0x0);
	_outb(PIC_SLAVE_IMR_PORT, 0x0);
}

static uint32_t tsc_per_sec = 0;
static uint32_t timer_delay = 0;

time_t timer_one_ms()
{
	return tsc_per_sec / 1000 / 128;
}

void timer_set_delay(uint32_t delay)
{
	timer_delay = delay;
}

time_t timer_get_count()
{
	volatile uint32_t* lapic = (volatile uint32_t*)APIC_VIRT_LOC;
	return lapic[APIC_TIMER_CUR_CNT];
}

time_t timer_get_elapsed()
{
	volatile uint32_t* lapic = (volatile uint32_t*)APIC_VIRT_LOC;
	const uint32_t count = lapic[APIC_TIMER_CUR_CNT];
	if (count > timer_delay)
	{
		kprintf("PTC: %u - COUNT: %u\n", timer_delay, count);
	}
	ASSERT(timer_delay >= count);
	return timer_delay - count;
}

void timer_start()
{
	volatile uint32_t* lapic = (volatile uint32_t*)APIC_VIRT_LOC;
	lapic[APIC_TIMER_INIT_REG] = timer_delay;
}

void timer_resume()
{
	volatile uint32_t* lapic = (volatile uint32_t*)APIC_VIRT_LOC;
	lapic[APIC_TIMER_INIT_REG] = lapic[APIC_TIMER_CUR_CNT];
}

void timer_stop()
{
	volatile uint32_t* lapic = (volatile uint32_t*)APIC_VIRT_LOC;
	lapic[APIC_TIMER_INIT_REG] = 0;
}

void timer_handler(uint64_t vector, uint64_t code)
{
	UNUSED(vector);
	UNUSED(code);

	extern void timer_interrupt(void);

	timer_interrupt();

	volatile uint32_t* lapic = (volatile uint32_t*)APIC_VIRT_LOC;
	lapic[APIC_EOI] = 0;
}

void apic_init()
{
	pic_init();

	if (!check_apic())
	{
		panic("Processor does not have local APIC!");
	}

	// Find out the APIC's base address
	const uint32_t apic_location = apic_base_address();
	kprintf("APIC Location: 0x%x \n", apic_location);

	// The manual says this will only be 0 when the cpuid command
	// says there is no APIC present, but we'll check anyway
	if (apic_location == 0)
	{
		// No APIC
		panic("No APIC present! \n");
	}

	// Lets map this 4KiB APIC location to the end of
	// the kernel's space. It needs to be mapped as
	// an uncacheable region otherwise problems will
	// happen.

	if (!virt_map_phys(kernel_table, APIC_VIRT_LOC, apic_location,
				PG_FLAG_RW | PG_FLAG_PWT | PG_FLAG_PCD, PAGE_SMALL))
	{
		panic("Failed APIC virtual mapping \n");
	}

	// Now we can access the APIC's registers from APIC_VIRT_LOC
	volatile uint32_t* apic_regs = (volatile uint32_t*)APIC_VIRT_LOC;
#ifdef DEBUG_APIC
	const uint32_t apic_version = apic_regs[APIC_VER_REG];
	kprintf("APIC VER: 0x%x \n", apic_version);
	const uint32_t max_lvt_entries = ((apic_version >> 16) & 0xFF) + 1;
	kprintf("APIC MAX LVTs: %u \n", max_lvt_entries);
#endif
	// Setup the APIC LVTs
	kprintf("TIMER VEC: 0x%x \n", apic_regs[APIC_TIMER_REG]);


	// Setup the APIC
	apic_regs[APIC_TIMER_REG] = APIC_DISABLE;
	apic_regs[APIC_PERF_CNT_REG] = APIC_NMI;
	apic_regs[APIC_LINT0_REG] = APIC_DISABLE;
	apic_regs[APIC_LINT1_REG] = APIC_DISABLE;
	apic_regs[APIC_TPR] = 0;

	// Enable the APIC
	enable_apic();	

	apic_regs[APIC_SPURIOUS_REG] = 39 | APIC_SOFT_EN;
	apic_regs[APIC_TIMER_REG] = 32;
	apic_regs[APIC_TIMER_DIV_REG] = 0xA; // Divide by 128

	// In order to figure out how fast the APIC timer is we need a
	// second reference. We'll use the old PIC timer for this

	// Adapted from: http://wiki.osdev.org/APIC_timer
	_outb(PC_SPEAKER, (_inb(PC_SPEAKER) & 0xFD) | 1);
	_outb(CMD_PORT, CMD_SEL_CH2 | CMD_HW_ONE_SHOT | CMD_LO_HI_BYTE);
	// We will delay for 10ms
	// The PIT's frequency is 1.193182 MHz
	// Therefore:
	//  Value = 10ms / (1 / (1.193182MHz))
	//        = 11932 = 0x2E9C
	_outb(CH2_DATA_PORT, 0x9C);
	_outb(CH2_DATA_PORT, 0x2E);

	const uint8_t tmp = _inb(PC_SPEAKER) & 0xFE;
	_outb(PC_SPEAKER, tmp);
	_outb(PC_SPEAKER, tmp|1);

	apic_regs[APIC_TIMER_INIT_REG] = 0xFFFFFFFF;

	// Wait until the PIT goes to 0
	while (!(_inb(PC_SPEAKER) & 0x20));
	
	// Stop the APIC timer
	const uint32_t count = apic_regs[APIC_TIMER_CUR_CNT];
	apic_regs[APIC_TIMER_INIT_REG] = 0;

	kprintf("Timer Count: 0x%x\n", count);
	const uint32_t diff = 0xFFFFFFFF - count;
	tsc_per_sec = diff*100*128;
#ifdef DEBUG_APIC
	const uint32_t tps = tsc_per_sec / 128;

	kprintf("Ticks per sec: %u - %u\n", tps, tsc_per_sec);
#endif

	// Install the timer interrupt handler
	interrupts_install_isr(32, timer_handler);	

	// Disable the PIC
	_outb(PIC_SLAVE_IMR_PORT, 0xFF);
	_outb(PIC_MASTER_IMR_PORT, 0xFF);
}
