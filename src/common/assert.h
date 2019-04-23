/***********************************************************************************************************************************
Assert Routines
***********************************************************************************************************************************/
#ifndef COMMON_ASSERT_H
#define COMMON_ASSERT_H

#include "common/error.h"

/***********************************************************************************************************************************
Asserts are used in test code to ensure that certain conditions are true.  They are ommitted from production builds.
***********************************************************************************************************************************/
#ifndef NDEBUG
    #define ASSERT(condition)                                                                                                      \
        do                                                                                                                         \
        {                                                                                                                          \
            if (!(condition))                                                                                                      \
                THROW_FMT(AssertError, "assertion '%s' failed", #condition);                                                       \
        }                                                                                                                          \
        while (0)
#else
    #define ASSERT(condition)
#endif

/***********************************************************************************************************************************
Checks are used in production builds to test very important conditions.  Be sure to limit use to the most critical cases.
***********************************************************************************************************************************/
#define CHECK(condition)                                                                                                           \
    do                                                                                                                             \
    {                                                                                                                              \
        if (!(condition))                                                                                                          \
            THROW_FMT(AssertError, "check '%s' failed", #condition);                                                               \
    }                                                                                                                              \
    while (0)

#endif
