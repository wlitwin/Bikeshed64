#ifndef __KLIB_H__
#define __KLIB_H__

#include "inttypes.h"

/* Clamps a value to a given range.
 *
 * Parameters:
 *    val - The current value
 *    min - The minimum value
 *    max - The maximum value
 *
 * Returns:
 *    val if it's inside the range, min if val is less
 *    than min and max if val is larger than max.
 */
static inline __attribute__((always_inline))
uint64_t clamp(uint64_t val, uint64_t min, uint64_t max)
{
	if (val < min) return min;
	if (val > max) return max;
	return val;
}

/* Zero out a region of memory
 *
 * Parameters:
 *    ptr - A pointer to the region of memory
 *    size - How many bytes to clear
 */
void memclr(void* ptr, uint64_t size);

/* Set each byte in a region of memory to a specific value
 *
 * Parameters:
 *    ptr - A pointer to the region of memory
 *    val - The value to set every byte to
 *    size - The number of bytes to set
 */
void memset(void* ptr, uint8_t val, uint64_t size);

/* Copy a region of memory. Assumes the two regions are
 * non-overlapping.
 *
 * Parameters:
 *    dst - A pointer to the destination region
 *    src - A pointer to the source region
 *    size - The number of bytes to copy
 *
 * Returns:
 *    A pointer to the destination region
 */
void* memcpy(void* dst, const void* src, uint64_t size);

#endif
