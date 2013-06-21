#ifndef __INT_TYPES_H__
#define __INT_TYPES_H__

#define NULL ((void*)0)

// Credit to:
// http://blogs.msdn.com/b/abhinaba/archive/2008/10/27/c-c-compile-time-asserts.aspx
#define COMPILE_ASSERT(x) extern int __dummy[ (int) (x)!=0 ]

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
