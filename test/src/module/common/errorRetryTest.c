/***********************************************************************************************************************************
Error Retry Message Test
***********************************************************************************************************************************/
#include "common/harnessTime.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("ErrorRetry"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retry (detail disabled)");
        {
            ErrorRetry *const retry = errRetryNew();

            TRY_BEGIN()
            {
                THROW(FormatError, "message1");
            }
            CATCH_ANY()
            {
                TEST_RESULT_VOID(errRetryAddP(retry), "add retry");
            }
            TRY_END();

            TRY_BEGIN()
            {
                THROW(KernelError, "message2");
            }
            CATCH_ANY()
            {
                TEST_RESULT_VOID(errRetryAddP(retry, errorType(), STR(errorMessage())), "add retry");
            }
            TRY_END();

            TEST_RESULT_BOOL(errRetryType(retry) == &FormatError, true, "error type");
            TEST_RESULT_STR_Z(
                errRetryMessage(retry),
                "message1\n"
                "[RETRY DETAIL OMITTED]",
                "error message");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("retry (detail enabled)");
        {
            hrnErrorRetryDetailEnable();

            TimeMSec timeList[] = {0, 50, 75, 150};
            hrnTimeMSecSet(timeList, LENGTH_OF(timeList));

            ErrorRetry *const retry = errRetryNew();

            TRY_BEGIN()
            {
                THROW(FormatError, "message1");
            }
            CATCH_ANY()
            {
                TEST_RESULT_VOID(errRetryAddP(retry), "add retry");
            }
            TRY_END();

            TRY_BEGIN()
            {
                THROW(FormatError, "message1");
            }
            CATCH_ANY()
            {
                TEST_RESULT_VOID(errRetryAddP(retry), "add retry");
            }
            TRY_END();

            TRY_BEGIN()
            {
                THROW(KernelError, "message2");
            }
            CATCH_ANY()
            {
                TEST_RESULT_VOID(errRetryAddP(retry), "add retry");
            }
            TRY_END();

            TRY_BEGIN()
            {
                THROW(ServiceError, "message1");
            }
            CATCH_ANY()
            {
                TEST_RESULT_VOID(errRetryAddP(retry), "add retry");
            }
            TRY_END();

            TEST_RESULT_BOOL(errRetryType(retry) == &FormatError, true, "error type");
            TEST_RESULT_STR_Z(
                errRetryMessage(retry),
                "message1\n"
                "    [FormatError] on 2 retries from 50-150ms: message1\n"
                "    [KernelError] on retry at 75ms: message2",
                "error message");
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
