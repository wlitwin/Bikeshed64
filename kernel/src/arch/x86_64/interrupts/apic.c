#include "apic.h"

#include "arch/x86_64/virt_memory/paging.h"

#include "arch/x86_64/cpuid.h"
#include "arch/x86_64/panic.h"
#include "arch/x86_64/kprintf.h"

#define APIC_BASE_MSR 0x1B
#define APIC_VIRT_LOC 0xFFFFFFFFFFFFF000
#define APIC_VER_REG (0x30/sizeof(uint32_t))
#define APIC_TIMER_REG (0x320/sizeof(uint32_t))
#define APIC_CMCI_REG (0x2F0/sizeof(uint32_t))
#define APIC_LINT0_REG (0x350/sizeof(uint32_t))
#define APIC_LINT1_REG (0x360/sizeof(uint32_t))
#define APIC_ERROR_REG (0x370/sizeof(uint32_t))
#define APIC_PERF_CNT_REG (0x340/sizeof(uint32_t))
#define APIC_THERM_SEN_REG (0x330/sizeof(uint32_t))

#define APIC_ESR (0x280/sizeof(uint32_t))

void apic_init()
{
	// First check if we have a local APIC
	uint32_t eax, edx;
	cpuid(0x1, &eax, &edx);

	// local APIC == bit 9
	if (!(edx & 0x100))
	{
		panic("This processor does not have a local APIC!");	
	}

	readmsr(APIC_BASE_MSR, &eax, &edx);

	// Turn off the local APIC by clearing bit 11
	eax &= ~(0x800);
	writemsr(APIC_BASE_MSR, eax, edx);

	kprintf("EAX: 0x%x - EDX: 0x%x \n", eax, edx);

	const uint32_t apic_location = eax & 0xFFFFF000;
	//const uint32_t can_disable_apic = eax & 0x800;
	//const uint32_t is_bootstrap_proc = eax & 0x100;

	kprintf("APIC Location: 0x%x \n", apic_location);

	if (apic_location == 0)
	{
		// No APIC
		panic("No APIC present! \n");
	}
	else
	{
		// Lets map this 4KiB APIC location to the end of
		// the kernel's space. It needs to be mapped as
		// an uncacheable region otherwise problems will
		// happen.

		if (!virt_map_phys(KERNEL_PML4, APIC_VIRT_LOC, apic_location,
						PG_FLAG_RW | PG_FLAG_PWT | PG_FLAG_PCD, PAGE_SMALL))
		{
			panic("Failed APIC virtual mapping \n");
		}

		// Now we can access the APIC's registers from APIC_VIRT_LOC
		uint32_t* apic_regs = (uint32_t*)APIC_VIRT_LOC;		
		const uint32_t apic_version = apic_regs[APIC_VER_REG];
		kprintf("APIC VER: 0x%x \n", apic_version);
		const uint32_t max_lvt_entries = ((apic_version >> 16) & 0xFF) + 1;
		kprintf("APIC MAX LVTs: %u \n", max_lvt_entries);

		// Setup the APIC LVTs
		kprintf("TIMER VEC: 0x%x \n", apic_regs[APIC_TIMER_REG]);

	}
	
}
