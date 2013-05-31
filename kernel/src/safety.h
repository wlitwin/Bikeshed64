#ifndef __SAFETY_H__
#define __SAFETY_H__

// Credit to:
// http://blogs.msdn.com/b/abhinaba/archive/2008/10/27/c-c-compile-time-asserts.aspx
#define COMPILE_ASSERT(x) extern int __dummy[ (int) (x)!=0 ]

#endif
