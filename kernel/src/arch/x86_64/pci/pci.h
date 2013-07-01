#ifndef __X86_64_PCI_H__
#define __X86_64_PCI_H__

#include "inttypes.h"

#define PCI_CONFIG_SPACE_PORT 0xCF8
#define PCI_CONFIG_DATA_PORT  0xCFC
#define ENABLE_PCI_CONFIG_SPACE 0x80000000

#define PCI_VENDOR_ID 0x0
#define PCI_DEVICE_ID 0x2
#define PCI_CLASS 0x8
#define PCI_CACHE_LINE_SIZE 0xC
#define PCI_LATENCY_TIMER 0xD
#define PCI_HEADER_TYPE 0xE
#define PCI_BIST 0xF
#define PCI_CLASS_REVISION 0x08 /* Highest 24-bits are class, low are revision */

// The format of CONFIG_ADDRESS is the following:
// bus << 16  |  device << 11  |  function <<  8  |  offset

// 31 	        30 - 24 	23 - 16 	15 - 11 	    10 - 8 	            7 - 2 	            1 - 0 
// Enable Bit 	Reserved 	Bus Number 	Device Number 	Function Number 	Register Number 	00 

// Useful wiki
// http://en.wikipedia.org/wiki/Input/Output_Base_Address

typedef struct PCIHeaderType0
{
	uint32_t bar_address[6];

	uint32_t bar_sizes[6];

	uint32_t cardbus_cis_pointer;
	uint16_t subsystem_vendor_id;
	uint16_t subsystem_id;

	uint32_t expansion_rom_address;
	uint8_t capabilities_pointer;
	uint8_t reserved[7];
	uint8_t interrupt_line;
	uint8_t interrupt_pin;
	uint8_t min_grant;
	uint8_t max_latency;
} header_type_0;

typedef struct PCIHeaderType1
{
	uint32_t bar_address_0;
	uint32_t bar_address_1;

	uint8_t primary_bus_number;
	uint8_t secondary_bus_number;
	uint8_t subordinate_bus_number;
	uint8_t secondary_latency_timer;

	uint8_t io_base;
	uint8_t io_limit;
	uint16_t secondary_status;

	uint16_t memory_base;
	uint16_t memory_limit;

	uint16_t prefetchable_memory_base;
	uint16_t prefetchable_memory_limit;

	uint32_t prefetchable_base_upper_32_bits;
	uint32_t prefetchable_memory_limit_upper_32_bits;

	uint16_t io_base_upper_16_bits;
	uint16_t io_limit_upper_16_bits;

	uint8_t capability_pointer;
	uint8_t reserved[3];

	uint32_t expansion_rom_base_address;
	uint8_t interrupt_line;
	uint8_t interrupt_pin;
	uint16_t bridge_control;
} header_type_1;

typedef struct PCIHeaderType2
{
	uint32_t cardbus_socket_base_address;

	uint8_t offset_of_capabilities_list;
	uint8_t reserved;
	uint16_t secondary_status;

	uint8_t pci_bus_number;
	uint8_t card_bus_number;
	uint8_t subordinate_bus_number;
	uint8_t cardbus_latency_timer;

	uint32_t memory_base_address_0;
	uint32_t memory_limit_0;

	uint32_t memory_base_address_1;
	uint32_t memory_limit_1;

	uint32_t io_base_address_0;
	uint32_t io_limit_0;

	uint32_t io_base_address_1;
	uint32_t io_limit_1;

	uint8_t interrupt_line;
	uint8_t interrupt_pin;

	uint16_t bridge_control;

	uint16_t subsystem_device_id;
	uint16_t subsystem_vendor_id;

	uint32_t pci_legacy_mode_base_address;
} header_type_2;

typedef struct PCIConfig
{
	uint8_t bus;
	uint8_t slot;
	uint8_t function;

	uint16_t vendor_id;
	uint16_t device_id;
	uint8_t base_class;
	uint8_t sub_class;
	uint8_t programming_if;
	uint8_t revision;
	uint8_t BIST;
	uint8_t header_type;
	uint8_t latency_timer;
	uint8_t cache_line_size;

	union 
	{
		header_type_0 type_0;	
		header_type_1 type_1;
		header_type_2 type_2;
	} h_type;
} pci_config_t;

/* Initialize the pci devices
 */
void pci_init(void);

/* The following functions read a 32-bit, 16-bit, or 8-bit value (respectively) 
 * from the PCI configuration space of a device on the specified bus, device, 
 * function and offset.
 */
uint32_t pci_config_read_long(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_config_read_short(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t  pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

/* The following functions write a 32-bit, 16-bit, or 8-bit value (respectively)
 * from the PCI configuration space on the specified PCI bus, device, function, and offset
 */
void pci_config_write_long(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t val);
void pci_config_write_short(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t val);
void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t val);

/* Search all the PCI devices found for a specific class or PCI device
 *
 * Returns NULL if no PCI device matching the class, sub_class and prog_if is found
 */
const pci_config_t* pci_find_by_class(uint8_t base_class, uint8_t sub_class, uint8_t prog_if);

/* Search for a PCI device by its vendor and class 
 *
 * Returns NULL if there is no device matching that vendor and device id on the
 * PCI bus
 */
const pci_config_t* pci_find_by_device(uint16_t vendor_id, uint16_t device_id);

/* Find out how much memory a PCI device requires
 */
uint32_t pci_get_memory_size(pci_config_t* config);

/* Scans all PCI devices on all buses
 */
void pci_scan_devices(void);

/* Dump all of the found devices to the serial output 
 */
void pci_dump_all_devices(void);

/* Dump a specific PCI device to serial output
 */
void pci_dump_device(uint8_t bus, uint8_t slot, uint8_t function);

#endif
