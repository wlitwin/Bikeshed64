#ifndef __SAFETY_H__
#define __SAFETY_H__

#include "kernel/panic.h"

// Credit to:
// http://blogs.msdn.com/b/abhinaba/archive/2008/10/27/c-c-compile-time-asserts.aspx
#define COMPILE_ASSERT(x) extern int __dummy[ (int) (x)!=0 ]

// http://stackoverflow.com/questions/5641427/how-to-make-preprocessor-generate-a-string-for-line-keyword
#define S(X) #X
#define SX(X) S(X)
#define S__LINE__ SX(__LINE__)

#define UNUSED(X) ((void)X)

#define ASSERT(X) if (!(X)) panic("ASSERT FAILED: " __FILE__ ":" S__LINE__)

#endif
