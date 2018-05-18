/***********************************************************************************************************************************
Assert Routines
***********************************************************************************************************************************/
#ifndef COMMON_ASSERT_H
#define COMMON_ASSERT_H

#include "common/error.h"

/***********************************************************************************************************************************
For very important asserts that are shipped with the production code
***********************************************************************************************************************************/
#define ASSERT(condition)                                                                                                          \
    do                                                                                                                             \
    {                                                                                                                              \
        if (!(condition))                                                                                                          \
            THROW_FMT(AssertError, "assertion '%s' failed", #condition);                                                           \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Used for assertions that should only be run when debugging.  Ideal for conditions that need to be tested during development but
be too expensive to ship with the production code.
***********************************************************************************************************************************/
#ifndef NDEBUG
    #define ASSERT_DEBUG(condition)                                                                                                \
        do                                                                                                                         \
        {                                                                                                                          \
            if (!(condition))                                                                                                      \
                THROW_FMT(AssertError, "debug assertion '%s' failed", #condition);                                                 \
        }                                                                                                                          \
        while (0)
#else
    #define ASSERT_DEBUG(condition)
#endif

#endif
