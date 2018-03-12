/***********************************************************************************************************************************
Debug Routines
***********************************************************************************************************************************/
#ifndef COMMON_DEBUG_H
#define COMMON_DEBUG_H

#include "common/log.h"

/***********************************************************************************************************************************
NDEBUG indicates to C library routines that debugging is off -- set a more readable flag to use when debugging is on
***********************************************************************************************************************************/
#ifndef NDEBUG
    #define DEBUG
#endif

/***********************************************************************************************************************************
Assert Macros

Used for assertions that should only be run when debugging.  Ideal for conditions that are not likely to happen in production but
could occur during development.
***********************************************************************************************************************************/
#ifdef DEBUG
    #define ASSERT_DEBUG(condition)                                                                                                \
    {                                                                                                                              \
        if (!(condition))                                                                                                          \
            THROW(AssertError, "assertion '%s' failed", #condition);                                                               \
    }
#else
    #define ASSERT_DEBUG(condition)
#endif

/***********************************************************************************************************************************
Extern variables that are needed for unit testing
***********************************************************************************************************************************/
#ifdef DEBUG_UNIT
    #define DEBUG_UNIT_EXTERN
#else
    #define DEBUG_UNIT_EXTERN                                                                                                      \
        static
#endif

#endif
