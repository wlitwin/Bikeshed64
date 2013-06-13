#include "apic.h"

#include "arch/x86_64/cpuid.h"
#include "arch/x86_64/kprintf.h"

#define APIC_BASE_MSR 0x1B

void apic_init()
{
	uint32_t eax, edx;
	readmsr(APIC_BASE_MSR, &eax, &edx);

	kprintf("EAX: 0x%x - EDX: 0x%x \n", eax, edx);

	const uint32_t apic_location = eax & 0xFFFFF000;
	const uint32_t can_disable_apic = eax & 0x800;
	const uint32_t is_bootstrap_proc = eax & 0x100;

	kprintf("APIC Location: 0x%x \n", apic_location);

	if (apic_location == 0)
	{
		// No APIC
	}
	
}
