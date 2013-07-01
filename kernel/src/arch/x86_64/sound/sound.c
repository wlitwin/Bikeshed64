#include "sound.h"

#include "arch/x86_64/kprintf.h"
#include "arch/x86_64/pci/pci.h"

void sound_init()
{
	kprintf("Initializing sound\n");
	pci_init();
	__asm__ volatile("hlt");
}
