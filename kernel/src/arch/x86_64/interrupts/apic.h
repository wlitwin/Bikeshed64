#ifndef __X86_64_INTERRUPTS_APIC_H__
#define __X86_64_INTERRUPTS_APIC_H__

/* Initializes the local APIC. Currently this feature of the AMD64
 * architecture is not supported. This initialization function simply
 * checks for its existence and then disables it. In the future 
 * this will be used but for simplicity and time constraints the old
 * style PIC is being used.
 */
void apic_init(void);

#endif
