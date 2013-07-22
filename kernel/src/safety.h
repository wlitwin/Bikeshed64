#ifndef __SAFETY_H__
#define __SAFETY_H__

/* This file contains a few macros for debugging and general safety-ness
 */

#include "kernel/panic.h"

// Credit to:
// http://blogs.msdn.com/b/abhinaba/archive/2008/10/27/c-c-compile-time-asserts.aspx
// Creates a compile time assert.
//
// Unfortunately the ouputted error message is not very helpful
//
// Parameters:
//    X - A boolean statement
#define COMPILE_ASSERT(x) extern int __dummy[ (int) (x)!=0 ]

// http://stackoverflow.com/questions/5641427/how-to-make-preprocessor-generate-a-string-for-line-keyword
// Converts a number literal to a string at compile time
#define S(X) #X
#define SX(X) S(X)
#define S__LINE__ SX(__LINE__)

/* Put this declaration inside a function where the variable is unused to prevent
 * the compiler from outputting a warning message.
 *
 * Parameters:
 *    VARNAME - The variable name
 */
#define UNUSED(VARNAME) ((void)VARNAME)

/* Runtime assert command. Will output the file and line number of the failed
 * assert statement.
 *
 * Parameters:
 *    X - A boolean statement
 */
#define ASSERT(X) if (!(X)) panic("ASSERT FAILED: " __FILE__ ":" S__LINE__)

#endif
