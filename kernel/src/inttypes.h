#ifndef __INT_TYPES_H__
#define __INT_TYPES_H__

/* Defines integer types and verifies they conform the proper size
 */

#include "safety.h"

#define NULL ((void*)0)

/* Align to a boundary
 *
 * Parameters:
 *    ADDR - The address to align
 *    P2 - The power of two to align it to
 */
#define ALIGN(ADDR,P2) ((ADDR+P2-1) & ~(P2 - 1))

typedef unsigned long uint64_t;
COMPILE_ASSERT(sizeof(uint64_t) == 8);

typedef unsigned int uint32_t;
COMPILE_ASSERT(sizeof(uint32_t) == 4);

typedef unsigned short uint16_t;
COMPILE_ASSERT(sizeof(uint16_t) == 2);

typedef unsigned char uint8_t;
COMPILE_ASSERT(sizeof(uint8_t) == 1);

typedef long int64_t;
COMPILE_ASSERT(sizeof(int64_t) == 8);

typedef int int32_t;
COMPILE_ASSERT(sizeof(int32_t) == 4);

typedef short int16_t;
COMPILE_ASSERT(sizeof(int16_t) == 2);

typedef char int8_t;
COMPILE_ASSERT(sizeof(int8_t) == 1);

COMPILE_ASSERT(sizeof(void*) == 8);

#endif
