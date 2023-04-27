/***********************************************************************************************************************************
Error Retry Message
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/error/retry.h"
#include "common/time.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ErrorRetry
{
    ErrorRetryPub pub;                                              // Publicly accessible variables
    String *messageLast;                                            // Last error message
    TimeMSec timeBegin;                                             // Time error retries began
};

/**********************************************************************************************************************************/
FN_EXTERN ErrorRetry *
errRetryNew(void)
{
    FUNCTION_TEST_VOID();

    OBJ_NEW_BEGIN(ErrorRetry, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (ErrorRetry)
        {
            .timeBegin = timeMSec(),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(ERROR_RETRY, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
errRetryAdd(ErrorRetry *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ERROR_RETRY, this);
    FUNCTION_TEST_END();

    // If the first error
    if (this->messageLast == NULL)
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            this->pub.type = errorType();
            this->pub.message = strCatZ(strNew(), errorMessage());
            this->messageLast = strCatZ(strNew(), errorMessage());
        }
        MEM_CONTEXT_OBJ_END();
    }
    // Else on each retry
    else
    {
        strCatFmt(
            this->pub.message, "\n[%s] on retry after %" PRIu64 "ms: ", errorTypeName(errorType()),
            timeMSec() - this->timeBegin);

        // If the message is the same as the last message
        if (strEqZ(this->messageLast, errorMessage()))
        {
            strCatZ(this->pub.message, "[same message]");
        }
        // Else append new message
        else
        {
            strCatZ(this->pub.message, errorMessage());

            // Update last message
            strTrunc(this->messageLast);
            strCatZ(this->messageLast, errorMessage());
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}
