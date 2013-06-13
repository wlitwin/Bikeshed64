#include "interrupts.h"

#include "tss.h"
#include "apic.h"
#include "safety.h"
#include "inttypes.h"

#include "kernel/klib.h"

#include "arch/x86_64/panic.h"
#include "arch/x86_64/kprintf.h"

#define IDT_SEG_PRESENT (0x1 << 15)

#define IDT_DPL0 (0x0 << 13)
#define IDT_DPL1 (0x1 << 13)
#define IDT_DPL2 (0x2 << 13)
#define IDT_DPL3 (0x3 << 13)

#define IDT_TYPE_INT_GATE (0xE << 7)

typedef struct
{
	uint16_t offset_L;
	uint16_t segment_selector;
	uint8_t ist;
	uint8_t flags;
	uint16_t offset_H;
	uint32_t offset_HH;
	uint32_t reserved;
} __attribute__((packed)) IDT_Gate;

COMPILE_ASSERT(sizeof(IDT_Gate) == 16);

#define GDT_CODE_SEG 0x10
#define GDT_DATA_SEG 0x20

#define idt start_idt_64
extern IDT_Gate start_idt_64[256];

typedef void (*interrupt_handler)(void);

void (*isr_table[256])(uint64_t vector, uint64_t code);

static void set_idt_entry(uint64_t index, interrupt_handler fn_ih)
{
	if (index > 255)
	{
		panic("Bad interrupt handler!");
	}

	const uint64_t fn_loc = (uint64_t)fn_ih;

	idt[index].offset_L = fn_loc & 0xFFFF;
	idt[index].segment_selector = GDT_CODE_SEG;
	idt[index].ist = 0;
	idt[index].flags = 0x8E;
	idt[index].offset_H = (fn_loc >> 16) & 0xFFFF;
	idt[index].offset_HH = (fn_loc >> 32) & 0xFFFFFFFF;
	idt[index].reserved = 0;
}

static void default_handler(uint64_t vector, uint64_t code)
{
	kprintf("Interrupt! vector: %u - Code: %u \n", vector, code);
}

void interrupt_install_isr(uint64_t index, void handler(uint64_t, uint64_t))
{
	isr_table[index] = handler;
}

void interrupts_init()
{
	setup_tss_descriptor();

	extern interrupt_handler isr_stub_table[256];

	for (uint64_t i = 0; i < 256; ++i)
	{
		set_idt_entry(i, isr_stub_table[i]);
		interrupt_install_isr(i, &default_handler);
	}

	// Initialize the APIC so interrupts from it don't
	// come to the exception interrupt vectors
	apic_init();

	__asm__ volatile ("sti");

//__asm__ volatile ("int $255");
//__asm__ volatile ("int $33");
/*__asm__ volatile ("int $34");
__asm__ volatile ("int $35");
__asm__ volatile ("int $36");
__asm__ volatile ("int $37");
__asm__ volatile ("int $38");
__asm__ volatile ("int $39");
__asm__ volatile ("int $40");
__asm__ volatile ("int $41");
__asm__ volatile ("int $42");
__asm__ volatile ("int $43");
__asm__ volatile ("int $44");
__asm__ volatile ("int $45");
__asm__ volatile ("int $46");
__asm__ volatile ("int $47");
__asm__ volatile ("int $48");
__asm__ volatile ("int $49");
__asm__ volatile ("int $50");
__asm__ volatile ("int $51");
__asm__ volatile ("int $52");
__asm__ volatile ("int $53");
__asm__ volatile ("int $54");
__asm__ volatile ("int $55");
__asm__ volatile ("int $56");
__asm__ volatile ("int $57");
__asm__ volatile ("int $58");
__asm__ volatile ("int $59");
__asm__ volatile ("int $60");
__asm__ volatile ("int $61");
__asm__ volatile ("int $62");
__asm__ volatile ("int $63");
__asm__ volatile ("int $64");
__asm__ volatile ("int $65");
__asm__ volatile ("int $66");
__asm__ volatile ("int $67");
__asm__ volatile ("int $68");
__asm__ volatile ("int $69");
__asm__ volatile ("int $70");
__asm__ volatile ("int $71");
__asm__ volatile ("int $72");
__asm__ volatile ("int $73");
__asm__ volatile ("int $74");
__asm__ volatile ("int $75");
__asm__ volatile ("int $76");
__asm__ volatile ("int $77");
__asm__ volatile ("int $78");
__asm__ volatile ("int $79");
__asm__ volatile ("int $80");
__asm__ volatile ("int $81");
__asm__ volatile ("int $82");
__asm__ volatile ("int $83");
__asm__ volatile ("int $84");
__asm__ volatile ("int $85");
__asm__ volatile ("int $86");
__asm__ volatile ("int $87");
__asm__ volatile ("int $88");
__asm__ volatile ("int $89");
__asm__ volatile ("int $90");
__asm__ volatile ("int $91");
__asm__ volatile ("int $92");
__asm__ volatile ("int $93");
__asm__ volatile ("int $94");
__asm__ volatile ("int $95");
__asm__ volatile ("int $96");
__asm__ volatile ("int $97");
__asm__ volatile ("int $98");
__asm__ volatile ("int $99");
__asm__ volatile ("int $100");
__asm__ volatile ("int $101");
__asm__ volatile ("int $102");
__asm__ volatile ("int $103");
__asm__ volatile ("int $104");
__asm__ volatile ("int $105");
__asm__ volatile ("int $106");
__asm__ volatile ("int $107");
__asm__ volatile ("int $108");
__asm__ volatile ("int $109");
__asm__ volatile ("int $110");
__asm__ volatile ("int $111");
__asm__ volatile ("int $112");
__asm__ volatile ("int $113");
__asm__ volatile ("int $114");
__asm__ volatile ("int $115");
__asm__ volatile ("int $116");
__asm__ volatile ("int $117");
__asm__ volatile ("int $118");
__asm__ volatile ("int $119");
__asm__ volatile ("int $120");
__asm__ volatile ("int $121");
__asm__ volatile ("int $122");
__asm__ volatile ("int $123");
__asm__ volatile ("int $124");
__asm__ volatile ("int $125");
__asm__ volatile ("int $126");
__asm__ volatile ("int $127");
__asm__ volatile ("int $128");
__asm__ volatile ("int $129");
__asm__ volatile ("int $130");
__asm__ volatile ("int $131");
__asm__ volatile ("int $132");
__asm__ volatile ("int $133");
__asm__ volatile ("int $134");
__asm__ volatile ("int $135");
__asm__ volatile ("int $136");
__asm__ volatile ("int $137");
__asm__ volatile ("int $138");
__asm__ volatile ("int $139");
__asm__ volatile ("int $140");
__asm__ volatile ("int $141");
__asm__ volatile ("int $142");
__asm__ volatile ("int $143");
__asm__ volatile ("int $144");
__asm__ volatile ("int $145");
__asm__ volatile ("int $146");
__asm__ volatile ("int $147");
__asm__ volatile ("int $148");
__asm__ volatile ("int $149");
__asm__ volatile ("int $150");
__asm__ volatile ("int $151");
__asm__ volatile ("int $152");
__asm__ volatile ("int $153");
__asm__ volatile ("int $154");
__asm__ volatile ("int $155");
__asm__ volatile ("int $156");
__asm__ volatile ("int $157");
__asm__ volatile ("int $158");
__asm__ volatile ("int $159");
__asm__ volatile ("int $160");
__asm__ volatile ("int $161");
__asm__ volatile ("int $162");
__asm__ volatile ("int $163");
__asm__ volatile ("int $164");
__asm__ volatile ("int $165");
__asm__ volatile ("int $166");
__asm__ volatile ("int $167");
__asm__ volatile ("int $168");
__asm__ volatile ("int $169");
__asm__ volatile ("int $170");
__asm__ volatile ("int $171");
__asm__ volatile ("int $172");
__asm__ volatile ("int $173");
__asm__ volatile ("int $174");
__asm__ volatile ("int $175");
__asm__ volatile ("int $176");
__asm__ volatile ("int $177");
__asm__ volatile ("int $178");
__asm__ volatile ("int $179");
__asm__ volatile ("int $180");
__asm__ volatile ("int $181");
__asm__ volatile ("int $182");
__asm__ volatile ("int $183");
__asm__ volatile ("int $184");
__asm__ volatile ("int $185");
__asm__ volatile ("int $186");
__asm__ volatile ("int $187");
__asm__ volatile ("int $188");
__asm__ volatile ("int $189");
__asm__ volatile ("int $190");
__asm__ volatile ("int $191");
__asm__ volatile ("int $192");
__asm__ volatile ("int $193");
__asm__ volatile ("int $194");
__asm__ volatile ("int $195");
__asm__ volatile ("int $196");
__asm__ volatile ("int $197");
__asm__ volatile ("int $198");
__asm__ volatile ("int $199");
__asm__ volatile ("int $200");
__asm__ volatile ("int $201");
__asm__ volatile ("int $202");
__asm__ volatile ("int $203");
__asm__ volatile ("int $204");
__asm__ volatile ("int $205");
__asm__ volatile ("int $206");
__asm__ volatile ("int $207");
__asm__ volatile ("int $208");
__asm__ volatile ("int $209");
__asm__ volatile ("int $210");
__asm__ volatile ("int $211");
__asm__ volatile ("int $212");
__asm__ volatile ("int $213");
__asm__ volatile ("int $214");
__asm__ volatile ("int $215");
__asm__ volatile ("int $216");
__asm__ volatile ("int $217");
__asm__ volatile ("int $218");
__asm__ volatile ("int $219");
__asm__ volatile ("int $220");
__asm__ volatile ("int $221");
__asm__ volatile ("int $222");
__asm__ volatile ("int $223");
__asm__ volatile ("int $224");
__asm__ volatile ("int $225");
__asm__ volatile ("int $226");
__asm__ volatile ("int $227");
__asm__ volatile ("int $228");
__asm__ volatile ("int $229");
__asm__ volatile ("int $230");
__asm__ volatile ("int $231");
__asm__ volatile ("int $232");
__asm__ volatile ("int $233");
__asm__ volatile ("int $234");
__asm__ volatile ("int $235");
__asm__ volatile ("int $236");
__asm__ volatile ("int $237");
__asm__ volatile ("int $238");
__asm__ volatile ("int $239");
__asm__ volatile ("int $240");
__asm__ volatile ("int $241");
__asm__ volatile ("int $242");
__asm__ volatile ("int $243");
__asm__ volatile ("int $244");
__asm__ volatile ("int $245");
__asm__ volatile ("int $246");
__asm__ volatile ("int $247");
__asm__ volatile ("int $248");
__asm__ volatile ("int $249");
__asm__ volatile ("int $250");
__asm__ volatile ("int $251");
__asm__ volatile ("int $252");
__asm__ volatile ("int $253");
__asm__ volatile ("int $254");
__asm__ volatile ("int $255");
*/

	__asm__ volatile ("cli");
	kprintf("Interrupt successfully returned! \n");
	__asm__ volatile ("hlt");
}


