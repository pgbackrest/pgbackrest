/***********************************************************************************************************************************
Wait Handler

Used for operations that may fail due to an error or some unexpected condition such as file missing. When waitMore() is called it
will wait (based on a Fibonacci backoff) before returning to give the error or condition time to clear. Even when the wait time has
expired before waitMore() is called, there will still be two retries to compensate for operations that use up the entire time limit.
Any number of retries are allowed within the time limit.
***********************************************************************************************************************************/
#ifndef COMMON_WAIT_H
#define COMMON_WAIT_H

/***********************************************************************************************************************************
Wait object
***********************************************************************************************************************************/
typedef struct Wait Wait;

#include "common/time.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN Wait *waitNew(TimeMSec waitTime);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Return the remaining time left
FN_EXTERN TimeMSec waitRemains(Wait *this);

// Wait and return true if the caller has more time/retries left
FN_EXTERN bool waitMore(Wait *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
waitFree(Wait *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_WAIT_TYPE                                                                                                     \
    Wait *
#define FUNCTION_LOG_WAIT_FORMAT(value, buffer, bufferSize)                                                                        \
    objNameToLog(value, "Wait", buffer, bufferSize)

#endif
