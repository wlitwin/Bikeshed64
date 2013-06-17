#ifndef __PANIC_H__
#define __PANIC_H__

/* Print a message and then halt the kernel.
 * Defined in architecture specific files.
 */
extern void panic(const char* message);

#endif
