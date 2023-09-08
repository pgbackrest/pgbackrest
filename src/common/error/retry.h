/***********************************************************************************************************************************
Error Retry Message

Accumulate errors during retries to provide a single, coherent error message to the user that includes information about the
original error and all the failed retries.
***********************************************************************************************************************************/
#ifndef COMMON_ERROR_RETRY_H
#define COMMON_ERROR_RETRY_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct ErrorRetry ErrorRetry;

#include "common/error/error.h"
#include "common/type/object.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
FN_EXTERN ErrorRetry *errRetryNew(void);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add retry error
FN_EXTERN void errRetryAdd(ErrorRetry *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct ErrorRetryPub
{
    const ErrorType *type;                                          // Error type
} ErrorRetryPub;

// Get error type
FN_INLINE_ALWAYS const ErrorType *
errRetryType(const ErrorRetry *const this)
{
    return THIS_PUB(ErrorRetry)->type;
}

// Get error message
FN_EXTERN String *errRetryMessage(const ErrorRetry *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
errRetryFree(ErrorRetry *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_ERROR_RETRY_TYPE                                                                                              \
    ErrorRetry *
#define FUNCTION_LOG_ERROR_RETRY_FORMAT(value, buffer, bufferSize)                                                                 \
    objNameToLog(value, "ErrorRetry", buffer, bufferSize)

#endif
