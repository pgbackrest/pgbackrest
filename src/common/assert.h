/***********************************************************************************************************************************
Assert Routines
***********************************************************************************************************************************/
#ifndef COMMON_ASSERT_H
#define COMMON_ASSERT_H

#include "common/error/error.h"

/***********************************************************************************************************************************
Asserts are used in test code to ensure that certain conditions are true. They are omitted from production builds.
***********************************************************************************************************************************/
#ifdef DEBUG

#define ASSERT(condition)                                                                                                          \
    do                                                                                                                             \
    {                                                                                                                              \
        if (!(condition))                                                                                                          \
            THROW_FMT(AssertError, "assertion '%s' failed", #condition);                                                           \
    }                                                                                                                              \
    while (0)

// Skip inline asserts when coverage testing because they will not have branch coverage. Generally speaking inline assertions
// should be of the "this != NULL" variety which is also caught effectively by Valgrind.
#ifndef DEBUG_COVERAGE

#define ASSERT_INLINE(condition)                                                                                                   \
    do                                                                                                                             \
    {                                                                                                                              \
        if (!(condition))                                                                                                          \
            THROW_FMT(AssertError, "assertion '%s' failed", #condition);                                                           \
    }                                                                                                                              \
    while (0)

#else
#define ASSERT_INLINE(condition)
#endif

// Used when execution reaches an invalid location rather than an invalid condition
#define ASSERT_MSG(message)                                                                                                        \
    THROW_FMT(AssertError, message);

// Declare variables that will be used by later assertions with the goal of making them easier to read and maintain
#define ASSERT_DECLARE(declaration)                                                                                                \
    declaration

// Add a parameter to a function that is only used by assertions
#define ASSERT_PARAM(param)                                                                                                        \
    , param

#else

#define ASSERT(condition)
#define ASSERT_INLINE(condition)
#define ASSERT_MSG(message)
#define ASSERT_DECLARE(declaration)
#define ASSERT_PARAM(param)

#endif

/***********************************************************************************************************************************
Checks are used in production builds to test very important conditions. Be sure to limit use to the most critical cases.
***********************************************************************************************************************************/
#define CHECK(type, condition, message)                                                                                            \
    do                                                                                                                             \
    {                                                                                                                              \
        if (!(condition))                                                                                                          \
            THROW(type, message);                                                                                                  \
    }                                                                                                                              \
    while (0)

#define CHECK_FMT(type, condition, message, ...)                                                                                   \
    do                                                                                                                             \
    {                                                                                                                              \
        if (!(condition))                                                                                                          \
            THROW_FMT(type, message, __VA_ARGS__);                                                                                 \
    }                                                                                                                              \
    while (0)

#endif
