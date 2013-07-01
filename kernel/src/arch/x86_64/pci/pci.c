#include "pci.h"
#include "arch/x86_64/panic.h"
#include "kernel/alloc/alloc.h"
#include "arch/x86_64/support.h"
#include "arch/x86_64/kprintf.h"
#include "kernel/data_structures/linkedlist.h"
#include "kernel/data_structures/watermark.h"
#include "safety.h"

static void panic_free(void* ptr)
{
	UNUSED(ptr);
	panic("Tried to free PCI list element");
}

static void* node_alloc(uint64_t size)
{
	return water_mark_alloc(&kernel_WaterMark, size);
}

static linked_list_t lst_pci_devices;

uint32_t pci_config_read_long(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
	const uint32_t address = ENABLE_PCI_CONFIG_SPACE | bus << 16 | device << 11 | 
						function << 8 | (offset & 0xFC);
	_outl(PCI_CONFIG_SPACE_PORT, address);
	uint32_t output = _inl(PCI_CONFIG_DATA_PORT);

	return output;
}

uint16_t pci_config_read_short(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
	const uint32_t address = ENABLE_PCI_CONFIG_SPACE | bus << 16 | device << 11 | 
					 function << 8 | (offset & 0xFC);
	_outl(PCI_CONFIG_SPACE_PORT, address);
	uint16_t output = (_inl(PCI_CONFIG_DATA_PORT) >> ((offset & 2) * 8)) & 0xffff;
	
	return output;
}

uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
	const uint32_t address = ENABLE_PCI_CONFIG_SPACE | bus << 16 | device << 11 | function << 8 | (offset & 0xFC);
	_outl(PCI_CONFIG_SPACE_PORT, address);
	uint8_t output = (_inl(PCI_CONFIG_DATA_PORT) >> ((offset & 2) * 8)) & 0xFF;
	
	return output;
}

void pci_config_write_long(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t val)
{
	const uint32_t pci = ENABLE_PCI_CONFIG_SPACE | bus << 16 | device << 11 | function << 8 | (offset & 0xFC);
	_outl(PCI_CONFIG_SPACE_PORT, pci);	
	_outl(PCI_CONFIG_DATA_PORT, val);
}

void pci_config_write_short(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t val)
{
	const uint32_t pci = ENABLE_PCI_CONFIG_SPACE | bus << 16 | device << 11 | function << 8 | (offset & 0xFC);
	_outl(PCI_CONFIG_SPACE_PORT, pci);
	_outw(PCI_CONFIG_DATA_PORT, val);// + (offset & 2));
}

void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t val)
{
	const uint32_t pci = ENABLE_PCI_CONFIG_SPACE | bus << 16 | device << 11 | function << 8 | (offset & 0xFC);
	_outl(PCI_CONFIG_SPACE_PORT, pci);	
	_outb(PCI_CONFIG_DATA_PORT, val);// + (offset & 3));
}

void pci_scan_devices()
{
	for (uint32_t bus = 0; bus < 256; ++bus)
	{
		for (uint32_t slot = 0; slot < 32; ++slot)
		{
			for (uint32_t func = 0; func < 8; ++func)
			{
				uint32_t vendor_id = pci_config_read_short(bus, slot, 0, PCI_VENDOR_ID);
				if (vendor_id == 0xFFFF) 
				{
					continue; 
				}

				uint32_t device_id = pci_config_read_short(bus, slot, func, PCI_DEVICE_ID);
				if (device_id == 0xFFFF)
				{
					continue;
				}

				// Okay this is a valid pci device!
				pci_config_t* pci_config = (pci_config_t *)water_mark_alloc(&kernel_WaterMark, sizeof(pci_config_t));
				ASSERT(pci_config != NULL);

				pci_config->bus  = bus;
				pci_config->slot = slot;
				pci_config->function = func;

				pci_config->vendor_id = vendor_id;
				pci_config->device_id = device_id;

				uint32_t class = pci_config_read_long(bus, slot, func, PCI_CLASS);
				pci_config->base_class     = (class >> 24) & 0xFF;
				pci_config->sub_class      = (class >> 16) & 0xFF;
				pci_config->programming_if = (class >> 8) & 0xFF;
				pci_config->revision       = class & 0xFF;

				pci_config->cache_line_size = pci_config_read_byte(bus, slot, func, PCI_CACHE_LINE_SIZE);
				pci_config->latency_timer = pci_config_read_byte(bus, slot, func, PCI_LATENCY_TIMER);
				pci_config->header_type = pci_config_read_byte(bus, slot, func, PCI_HEADER_TYPE);
				pci_config->BIST = pci_config_read_byte(bus, slot, func, PCI_BIST);

				// Mask off the special MF flag
				switch ((pci_config->header_type & 0x7))
				{
					case 0:
						{
							uint32_t offset = 0x10;
							for (int32_t i = 0; i < 6; ++i, offset += 4)
							{
								pci_config->h_type.type_0.bar_address[i] = pci_config_read_long(bus, slot, func, offset);
							}
							pci_config->h_type.type_0.cardbus_cis_pointer = pci_config_read_long(bus, slot, func, 0x28);
							pci_config->h_type.type_0.subsystem_vendor_id = pci_config_read_short(bus, slot, func, 0x2C);
							pci_config->h_type.type_0.subsystem_id = pci_config_read_short(bus, slot, func, 0x2E);
							pci_config->h_type.type_0.expansion_rom_address = pci_config_read_long(bus, slot, func, 0x30);
							pci_config->h_type.type_0.capabilities_pointer = pci_config_read_byte(bus, slot, func, 0x34);
							pci_config->h_type.type_0.interrupt_line = pci_config_read_byte(bus, slot, func, 0x3C);
							pci_config->h_type.type_0.interrupt_pin = pci_config_read_byte(bus, slot, func, 0x3D);
							pci_config->h_type.type_0.min_grant = pci_config_read_byte(bus, slot, func, 0x3E);
							pci_config->h_type.type_0.max_latency = pci_config_read_byte(bus, slot, func, 0x3F);

							pci_get_memory_size(pci_config);
						}
						break;
					case 1:
						{
							pci_config->h_type.type_1.bar_address_0 = pci_config_read_long(bus, slot, func, 0x10);
							pci_config->h_type.type_1.bar_address_1 = pci_config_read_long(bus, slot, func, 0x14);
							pci_config->h_type.type_1.primary_bus_number = pci_config_read_byte(bus, slot, func, 0x18);
							pci_config->h_type.type_1.secondary_bus_number = pci_config_read_byte(bus, slot, func, 0x19);
							pci_config->h_type.type_1.subordinate_bus_number = pci_config_read_byte(bus, slot, func, 0x1A);
							pci_config->h_type.type_1.secondary_latency_timer = pci_config_read_byte(bus, slot, func, 0x1B);
							pci_config->h_type.type_1.io_base = pci_config_read_byte(bus, slot, func, 0x1C);
							pci_config->h_type.type_1.io_limit = pci_config_read_byte(bus, slot, func, 0x1D);
							pci_config->h_type.type_1.secondary_status = pci_config_read_short(bus, slot, func, 0x1E);
							pci_config->h_type.type_1.memory_base = pci_config_read_short(bus, slot, func, 0x20);
							pci_config->h_type.type_1.memory_limit = pci_config_read_short(bus, slot, func, 0x22);
							pci_config->h_type.type_1.prefetchable_memory_base = pci_config_read_short(bus, slot, func, 0x24);
							pci_config->h_type.type_1.prefetchable_memory_limit = pci_config_read_short(bus, slot, func, 0x26);
							pci_config->h_type.type_1.prefetchable_base_upper_32_bits = pci_config_read_long(bus, slot, func, 0x28);
							pci_config->h_type.type_1.prefetchable_memory_limit_upper_32_bits = pci_config_read_long(bus, slot, func, 0x2C);
							pci_config->h_type.type_1.io_base_upper_16_bits = pci_config_read_short(bus, slot, func, 0x30);
							pci_config->h_type.type_1.io_limit_upper_16_bits = pci_config_read_short(bus, slot, func, 0x32);
							pci_config->h_type.type_1.capability_pointer = pci_config_read_byte(bus, slot, func, 0x34);
							pci_config->h_type.type_1.expansion_rom_base_address = pci_config_read_long(bus, slot, func, 0x38);
							pci_config->h_type.type_1.interrupt_line = pci_config_read_byte(bus, slot, func, 0x3C);
							pci_config->h_type.type_1.interrupt_pin = pci_config_read_byte(bus, slot, func, 0x3D);
							pci_config->h_type.type_1.bridge_control = pci_config_read_short(bus, slot, func, 0x3E);
						}
						break;
					case 2:
						{
							pci_config->h_type.type_2.cardbus_socket_base_address = pci_config_read_long(bus, slot, func, 0x10);
							pci_config->h_type.type_2.offset_of_capabilities_list = pci_config_read_byte(bus, slot, func, 0x14);
							pci_config->h_type.type_2.secondary_status = pci_config_read_short(bus, slot, func, 0x16);
							pci_config->h_type.type_2.pci_bus_number = pci_config_read_byte(bus, slot, func, 0x18);
							pci_config->h_type.type_2.card_bus_number = pci_config_read_byte(bus, slot, func, 0x19);
							pci_config->h_type.type_2.subordinate_bus_number = pci_config_read_byte(bus, slot, func, 0x1A);
							pci_config->h_type.type_2.cardbus_latency_timer = pci_config_read_byte(bus, slot, func, 0x1B);
							pci_config->h_type.type_2.memory_base_address_0 = pci_config_read_long(bus, slot, func, 0x1C);
							pci_config->h_type.type_2.memory_limit_0 = pci_config_read_long(bus, slot, func, 0x20);
							pci_config->h_type.type_2.memory_base_address_1 = pci_config_read_long(bus, slot, func, 0x24);
							pci_config->h_type.type_2.memory_limit_1 = pci_config_read_long(bus, slot, func, 0x28);
							pci_config->h_type.type_2.io_base_address_0 = pci_config_read_long(bus, slot, func, 0x2C);
							pci_config->h_type.type_2.io_limit_0 = pci_config_read_long(bus, slot, func, 0x30);
							pci_config->h_type.type_2.io_base_address_1 = pci_config_read_long(bus, slot, func, 0x34);
							pci_config->h_type.type_2.io_limit_1 = pci_config_read_long(bus, slot, func, 0x38);
							pci_config->h_type.type_2.interrupt_line = pci_config_read_byte(bus, slot, func, 0x3C);
							pci_config->h_type.type_2.interrupt_pin = pci_config_read_byte(bus, slot, func, 0x3D);
							pci_config->h_type.type_2.bridge_control = pci_config_read_short(bus, slot, func, 0x3E);
							pci_config->h_type.type_2.subsystem_device_id = pci_config_read_short(bus, slot, func, 0x40);
							pci_config->h_type.type_2.subsystem_vendor_id = pci_config_read_short(bus, slot, func, 0x42);
							pci_config->h_type.type_2.pci_legacy_mode_base_address = pci_config_read_long(bus, slot, func, 0x44);
						}
						break;
					default:
						// Uh oh?
						panic("PCI - Device found with bad header type");
						break;
				}

				// Add this device to our devices list
				list_insert_next(&lst_pci_devices, NULL, pci_config);
			}
		}
	}
}

uint32_t pci_get_memory_size(pci_config_t* config)
{
	switch (config->header_type & 0x7)
	{
		case 0:
			{
				//pci_config->h_type.type_0.bar_address_0 = pci_config_read_long(bus, slot, func, 0x10);
				//pci_config->h_type.type_0.bar_address_1 = pci_config_read_long(bus, slot, func, 0x14);
				//pci_config->h_type.type_0.bar_address_2 = pci_config_read_long(bus, slot, func, 0x18);
				//pci_config->h_type.type_0.bar_address_3 = pci_config_read_long(bus, slot, func, 0x1C);
				//pci_config->h_type.type_0.bar_address_4 = pci_config_read_long(bus, slot, func, 0x20);
				//pci_config->h_type.type_0.bar_address_5 = pci_config_read_long(bus, slot, func, 0x24);
				for (uint32_t offset = 0x10; offset <= 0x24; offset += 4)
				{
					uint32_t orig = pci_config_read_long(config->bus, config->slot, config->function, offset);
					pci_config_write_long(config->bus, config->slot, config->function, offset, 0xFFFFFFFF);
					uint32_t size = pci_config_read_long(config->bus, config->slot, config->function, offset);
					pci_config_write_long(config->bus, config->slot, config->function, offset, orig);

					size &= 0xFFFFFFF0;
					size = ~size;
					size += 1;

					int32_t index = (offset - 0x10) / 4;

					config->h_type.type_0.bar_sizes[index] = size;

					kprintf("Orig 0x%x: %x\n", offset, orig);
					kprintf("Memory size required: 0x%x\n", size);
					/* We need to mark these addresses as used, and identity map them! */
					//void* start_addr = (void *)(orig & 0xFFFFF000);
					// Skip addresses that are 0x0 or below 1MiB which is already identity mapped
				/*	if (start_addr != (void *)0x0 && start_addr > (void *)0x100000)
					{
						void* end_addr = start_addr + size;
						for (; start_addr < end_addr; start_addr += PAGE_SIZE)
						{
							__phys_set_bit(start_addr);
							__virt_map_page(start_addr, start_addr, PG_READ_WRITE);
						}
					}*/
				}

			}
			break;
		case 1:
			{

			}
			break;
		case 2:
			{

			}
			break;
	}

	return 0;
}

void pci_dump_all_devices()
{
	list_element_t* node = list_head(&lst_pci_devices);			

	kprintf("\nPCI devices list:\n");

	while (node != NULL)
	{
		pci_config_t* config = (pci_config_t *)list_data(node);

		kprintf("Bus: %d, Slot: %d, Function: %d\n", config->bus, config->slot, config->function);
		kprintf("Vendor ID    : %04xh\n", config->vendor_id);
		kprintf("Device ID    : %04xh\n", config->device_id);
		kprintf("Base Class   : %02xh\n", config->base_class);
		kprintf("Sub  Class   : %02xh\n", config->sub_class);
		kprintf("Prog IF      : %02xh\n", config->programming_if);
		kprintf("Revision     : %02xh\n", config->revision);
		kprintf("Cache Size   : %02xh\n", config->cache_line_size);
		kprintf("Latency Timer: %02xh\n", config->latency_timer);
		kprintf("Header Type  : %02xh\n", config->header_type);
		kprintf("BIST         : %02xh\n", config->BIST);

		switch ((config->header_type & 0x7))
		{
			case 0:
				{
					kprintf("\nHeader type: 0\n");	
					for (int32_t i = 0; i < 6; ++i)
					{
						kprintf("Bar address %d: %x\n", i, config->h_type.type_0.bar_address[i]);
					}
					kprintf("Carbus pointer: %x\n", config->h_type.type_0.cardbus_cis_pointer);
					kprintf("Subsystem Vendor ID: %x\n", config->h_type.type_0.subsystem_vendor_id);
					kprintf("Subsystem ID: %x\n", config->h_type.type_0.subsystem_id);
					kprintf("Expansion ROM addr: %x\n", config->h_type.type_0.expansion_rom_address);
					kprintf("Capabilities Pointer: %x\n", config->h_type.type_0.capabilities_pointer);
					kprintf("Interrupt line: %x\n", config->h_type.type_0.interrupt_line);
					kprintf("Interrupt pin: %x\n", config->h_type.type_0.interrupt_pin);
					kprintf("Min grant: %x\n", config->h_type.type_0.min_grant);
					kprintf("Max latency: %x\n", config->h_type.type_0.max_latency);
				}
				break;
			case 1:
				{
					kprintf("\nHeader type: 1\n");
					kprintf("Bar address 0: %x\n", config->h_type.type_1.bar_address_0);
					kprintf("Bar address 1: %x\n", config->h_type.type_1.bar_address_1);
					kprintf("Primary bus number: %x\n", config->h_type.type_1.primary_bus_number);
					kprintf("Secondary bus number: %x\n", config->h_type.type_1.secondary_bus_number);
					kprintf("Subordinate bus number: %x\n", config->h_type.type_1.subordinate_bus_number);
					kprintf("Secondary latency timer: %x\n", config->h_type.type_1.secondary_latency_timer);
					kprintf("IO base: %x\n", config->h_type.type_1.io_base);
					kprintf("IO limit: %x\n", config->h_type.type_1.io_limit);
					kprintf("Secondary status: %x\n", config->h_type.type_1.secondary_status);
					kprintf("Memory base: %x\n", config->h_type.type_1.memory_base);
					kprintf("Memory limit: %x\n", config->h_type.type_1.memory_limit);
					kprintf("Prefetch memory base: %x\n", config->h_type.type_1.prefetchable_memory_base);
					kprintf("Prefetch memory limit: %x\n", config->h_type.type_1.prefetchable_memory_limit);
					kprintf("Prefetch base upper 32-bits: %x\n", config->h_type.type_1.prefetchable_base_upper_32_bits);
					kprintf("Prefetch limit upper 32-bits: %x\n", config->h_type.type_1.prefetchable_memory_limit_upper_32_bits);
					kprintf("IO base upper 16-bits: %x\n", config->h_type.type_1.io_base_upper_16_bits);
					kprintf("IO limit upper 16-bits: %x\n", config->h_type.type_1.io_limit_upper_16_bits);
					kprintf("Capability pointer: %x\n", config->h_type.type_1.capability_pointer);
					kprintf("Expansion ROM base address: %x\n", config->h_type.type_1.expansion_rom_base_address);
					kprintf("Interrupt line: %x\n", config->h_type.type_1.interrupt_line);
					kprintf("Interrupt pin: %x\n", config->h_type.type_1.interrupt_pin);
					kprintf("Bridge control: %x\n", config->h_type.type_1.bridge_control);
				}
				break;
			case 2:
				{
					kprintf("Cardbus socket base address: %x\n", config->h_type.type_2.cardbus_socket_base_address);
					kprintf("Offset of capabilities: %x\n", config->h_type.type_2.offset_of_capabilities_list);
					kprintf("Secondary status: %x\n", config->h_type.type_2.secondary_status);
					kprintf("PCI bus number: %x\n", config->h_type.type_2.pci_bus_number);
					kprintf("Cardbus bus number: %x\n", config->h_type.type_2.card_bus_number);
					kprintf("Subordinate bus number: %x\n", config->h_type.type_2.subordinate_bus_number);
					kprintf("Cardbus latency timer: %x\n", config->h_type.type_2.cardbus_latency_timer);
					kprintf("Memory base address 0: %x\n", config->h_type.type_2.memory_base_address_0);
					kprintf("Memory limit 0: %x\n", config->h_type.type_2.memory_limit_0);
					kprintf("Memory base address 1: %x\n", config->h_type.type_2.memory_base_address_1);
					kprintf("Memory limit 1: %x\n", config->h_type.type_2.memory_limit_1);
					kprintf("IO base address 0: %x\n", config->h_type.type_2.io_base_address_0);
					kprintf("IO limit 0: %x\n", config->h_type.type_2.io_limit_0);
					kprintf("IO base address 1: %x\n", config->h_type.type_2.io_base_address_1);
					kprintf("IO limit 1: %x\n", config->h_type.type_2.io_limit_1);
					kprintf("Interrupt line: %x\n", config->h_type.type_2.interrupt_line);
					kprintf("Interrupt pin: %x\n", config->h_type.type_2.interrupt_pin);
					kprintf("Bridge control: %x\n", config->h_type.type_2.bridge_control);
					kprintf("Subsystem device ID: %x\n", config->h_type.type_2.subsystem_device_id);
					kprintf("Subsystem vendor ID: %x\n", config->h_type.type_2.subsystem_vendor_id);
					kprintf("16-bit PC card legacy base address: %x\n", config->h_type.type_2.pci_legacy_mode_base_address);
				}
				break;
		}

		kprintf("\n");
		node = list_next(node);
	}
}

const pci_config_t* pci_find_by_class(uint8_t base_class, uint8_t sub_class, uint8_t prog_if)
{
	list_element_t* node = list_head(&lst_pci_devices);

	while (node != NULL)
	{
		pci_config_t* pci_info = (pci_config_t *)list_data(node);
		if (pci_info->base_class == base_class && 
				pci_info->sub_class == sub_class &&
				pci_info->programming_if == prog_if)
		{
			return pci_info;
		}

		node = list_next(node);
	}

	return NULL;
}

const pci_config_t* pci_find_by_device(uint16_t vendor_id, uint16_t device_id)
{
	list_element_t* node = list_head(&lst_pci_devices);

	while (node != NULL)
	{
		pci_config_t* pci_info = (pci_config_t *)list_data(node);
		if (pci_info->vendor_id == vendor_id && pci_info->device_id == device_id)
		{
			return pci_info;
		}

		node = list_next(node);
	}

	return NULL;
}

void pci_dump_device(uint8_t bus, uint8_t slot, uint8_t func)
{
	//kprintf("pci 0000:%d:%d.%d", bus, slot, func);
	UNUSED(bus);
	UNUSED(slot);
	UNUSED(func);
}

void pci_init()
{
	list_init(&lst_pci_devices, node_alloc, panic_free);	
	pci_scan_devices();
	pci_dump_all_devices();	
	if (pci_find_by_device(0x8086, 0x2415) != NULL)
	{
		kprintf("Sound device present!\n");
	}
	else
	{
		kprintf("No sound device found!\n");
	}
	__asm__ volatile("hlt");
}

