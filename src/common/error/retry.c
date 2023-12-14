/***********************************************************************************************************************************
Error Retry Message
***********************************************************************************************************************************/
#include "build.auto.h"

#include "string.h"

#include "common/debug.h"
#include "common/error/retry.h"
#include "common/time.h"
#include "common/type/list.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ErrorRetry
{
    ErrorRetryPub pub;                                              // Publicly accessible variables
    const String *message;                                          // First error message
    List *list;                                                     // List of retry errors
    TimeMSec timeBegin;                                             // Time error retries began
};

// Keep track of retries with the same error
typedef struct ErrorRetryItem
{
    String *message;                                                // Error message
    unsigned int total;                                             // Total errors with this message
    const ErrorType *type;                                          // Error type
    TimeMSec retryFirst;                                            // First retry
    TimeMSec retryLast;                                             // Last Retry
} ErrorRetryItem;

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
            .list = lstNewP(sizeof(ErrorRetryItem), .comparator = lstComparatorStr),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(ERROR_RETRY, this);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
errRetryMessage(const ErrorRetry *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ERROR_RETRY, this);
    FUNCTION_TEST_END();

    ASSERT(this->message != NULL);

    // Always include first message
    String *const result = strCat(strNew(), this->message);

    // Add retries, varying the message depending on how many retries there were
    for (unsigned int listIdx = 0; listIdx < lstSize(this->list); listIdx++)
    {
        const ErrorRetryItem *const error = lstGet(this->list, listIdx);

        strCatFmt(result, "\n    [%s] ", errorTypeName(error->type));

        if (error->retryFirst == error->retryLast)
            strCatFmt(result, "on retry at %" PRIu64, error->retryFirst);
        else
            strCatFmt(result, "on %u retries from %" PRIu64 "-%" PRIu64, error->total, error->retryFirst, error->retryLast);

        strCatFmt(result, "ms: %s", strZ(error->message));
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
errRetryAdd(ErrorRetry *const this, const ErrRetryAddParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ERROR_RETRY, this);
        FUNCTION_TEST_PARAM(ERROR_TYPE, param.type);
        FUNCTION_TEST_PARAM(STRING, param.message);
    FUNCTION_TEST_END();

    // Set defaults
    const ErrorType *const type = param.type == NULL ? errorType() : param.type;
    const char *const message = param.message == NULL ? errorMessage() : strZ(param.message);

    // Set type on first error
    if (this->pub.type == NULL)
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            this->pub.type = type;
            this->message = strNewZ(message);
        }
        MEM_CONTEXT_OBJ_END();
    }
    else
    {
        // If error is not found then add it
        const String *const messageFind = STR(message);
        const TimeMSec retryTime = timeMSec() - this->timeBegin;
        ErrorRetryItem *const error = lstFind(this->list, &messageFind);

        if (error == NULL)
        {
            MEM_CONTEXT_OBJ_BEGIN(this->list)
            {
                const ErrorRetryItem errorNew =
                {
                    .type = type,
                    .total = 1,
                    .message = strDup(messageFind),
                    .retryFirst = retryTime,
                    .retryLast = retryTime,
                };

                lstAdd(this->list, &errorNew);
            }
            MEM_CONTEXT_OBJ_END();
        }
        // Else update the error found
        else
        {
            error->total++;
            error->retryLast = retryTime;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}
