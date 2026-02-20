/***********************************************************************************************************************************
General Macros

Portions Copyright (c) 1996-2026, PostgreSQL Global Development Group
Portions Copyright (c) 1994, Regents of the University of California
***********************************************************************************************************************************/
#ifndef COMMON_MACRO_H
#define COMMON_MACRO_H

#include <assert.h>
#include <stdalign.h>

/***********************************************************************************************************************************
Convert the parameter to a zero-terminated string

Useful for converting non-string types (e.g. int) to strings for inclusion in messages.
***********************************************************************************************************************************/
#define STRINGIFY_HELPER(param)                                     #param
#define STRINGIFY(param)                                            STRINGIFY_HELPER(param)

/***********************************************************************************************************************************
If param2 > param1 then assign it to param1

Useful for ensuring coverage in cases where compared values may be always ascending or descending.
***********************************************************************************************************************************/
#define MAX_ASSIGN(param1, param2)                                                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
        if (param2 > param1)                                                                                                       \
            param1 = param2;                                                                                                       \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
C11 supports static_assert but there are some syntactic placement restrictions. STATIC_ASSERT_EXP() makes it safe to use in an
expression.
***********************************************************************************************************************************/
#define STATIC_ASSERT_EXPR(condition, message)                                                                                     \
    ((void)({static_assert(condition, message); true;}))

/***********************************************************************************************************************************
Allows casting const-ness away from an expression, but doesn't allow changing the type. Enforcement of the latter currently only
works for gcc-like compilers.

Note that it is not safe to cast const-ness away if the result will ever be modified (it would be undefined behavior). Doing so can
cause compiler mis-optimizations or runtime crashes (by modifying read-only memory). It is only safe to use when the result will not
be modified, but API design or language restrictions prevent you from declaring that (e.g. because a function returns both const and
non-const variables).

Note that this only works in function scope, not for global variables (it would be nice, but not trivial, to improve that).

Adapted from PostgreSQL src/include/c.h.
***********************************************************************************************************************************/
#ifdef HAVE_BUILTIN_TYPES_COMPATIBLE_P
#define UNCONSTIFY(type, expression)                                                                                               \
    (STATIC_ASSERT_EXPR(__builtin_types_compatible_p(__typeof(expression), const type), "invalid cast"), (type)(expression))
#else
#define UNCONSTIFY(type, expression)                                                                                               \
    ((type)(expression))
#endif

/***********************************************************************************************************************************
Determine the byte offset required to align a type after an arbitrary number of bytes

This is useful for determining how to correctly align a type in a buffer that is being dynamically built up like a struct.
***********************************************************************************************************************************/
#define ALIGN_OFFSET(type, bytes) (alignof(type) - ((bytes) % alignof(type)))

/***********************************************************************************************************************************
Determine size of a type in a struct

Using sizeof() on a struct member requires additional syntax.
***********************************************************************************************************************************/
#define SIZE_OF_STRUCT_MEMBER(struct, member) sizeof(((struct){0}).member)

/***********************************************************************************************************************************
Determine the length of an array that can be determined at compile time

For this macro to work correctly the array must be declared like:

int intList[] = {0, 1};

It will not work for an array declared like:

int *intList;
***********************************************************************************************************************************/
#define LENGTH_OF(array) (sizeof(array) / sizeof((array)[0]))

#endif
