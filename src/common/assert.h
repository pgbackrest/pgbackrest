/***********************************************************************************************************************************
Assert Routines
***********************************************************************************************************************************/
#ifndef COMMON_ASSERT_H
#define COMMON_ASSERT_H

#include "common/error.h"

/***********************************************************************************************************************************
Asserts are used in test code to ensure that certain conditions are true.  They are omitted from production builds.
***********************************************************************************************************************************/
#ifndef NDEBUG
    #define ASSERT(condition)                                                                                                      \
        do                                                                                                                         \
        {                                                                                                                          \
            if (!(condition))                                                                                                      \
                THROW_FMT(AssertError, "assertion '%s' failed", #condition);                                                       \
        }                                                                                                                          \
        while (0)

    // Skip inline asserts when coverage testing because they will not have branch coverage. Generally speaking inline assertions
    // should be of the "this != NULL" variety which is also caught effectively by Valgrind.
    #ifndef DEBUG_COVERAGE
        #define ASSERT_INLINE(condition)                                                                                           \
            do                                                                                                                     \
            {                                                                                                                      \
                if (!(condition))                                                                                                  \
                    THROW_FMT(AssertError, "assertion '%s' failed", #condition);                                                   \
            }                                                                                                                      \
            while (0)
    #else
        #define ASSERT_INLINE(condition)
    #endif
#else
    #define ASSERT(condition)
    #define ASSERT_INLINE(condition)
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
