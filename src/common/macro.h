/***********************************************************************************************************************************
General Macros

Portions Copyright (c) 1996-2021, PostgreSQL Global Development Group
Portions Copyright (c) 1994, Regents of the University of California
***********************************************************************************************************************************/
#ifndef COMMON_MACRO_H
#define COMMON_MACRO_H

/***********************************************************************************************************************************
Convert the parameter to a zero-terminated string

Useful for converting non-string types (e.g. int) to strings for inclusion in messages.
***********************************************************************************************************************************/
#define STRINGIFY_HELPER(param)                                     #param
#define STRINGIFY(param)                                            STRINGIFY_HELPER(param)

/***********************************************************************************************************************************
Glue together a string/macro and another string//macro

Useful for creating function names when one or both of the macro parameter needs to be converted to a macro before concatenating.
common/type/object.h has numerous examples of this.
***********************************************************************************************************************************/
#define GLUE_HELPER(param1, param2)                                 param1##param2
#define GLUE(param1, param2)                                        GLUE_HELPER(param1, param2)

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
If the "condition" (a compile-time-constant expression) evaluates to false then throw a compile error using the "message" (a string
literal).

gcc 4.6 and up supports _Static_assert(), but there are bizarre syntactic placement restrictions.  Macros STATIC_ASSERT_STMT() and
STATIC_ASSERT_EXP() make it safe to use as a statement or in an expression, respectively.

Otherwise we fall back on a kluge that assumes the compiler will complain about a negative width for a struct bit-field.  This will
not include a helpful error message, but it beats not getting an error at all. Note that when std=c99 it looks like gcc is using the
same kluge.

Adapted from PostgreSQL src/include/c.h.
***********************************************************************************************************************************/
#ifdef HAVE_STATIC_ASSERT
    #define STATIC_ASSERT_STMT(condition, message)                                                                                 \
        do {_Static_assert(condition, message);} while (0)

    #define STATIC_ASSERT_EXPR(condition, message)                                                                                 \
        ((void)({STATIC_ASSERT_STMT(condition, message); true;}))
#else
    #define STATIC_ASSERT_STMT(condition, message)                                                                                 \
        ((void)sizeof(struct {int static_assert_failure : (condition) ? 1 : -1;}))

    #define STATIC_ASSERT_EXPR(condition, message)                                                                                 \
        STATIC_ASSERT_STMT(condition, message)
#endif

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
    #define UNCONSTIFY(type, expression)                                                                                           \
        (STATIC_ASSERT_EXPR(__builtin_types_compatible_p(__typeof(expression), const type), "invalid cast"), (type)(expression))
#else
    #define UNCONSTIFY(type, expression)                                                                                           \
        ((type)(expression))
#endif

#endif
