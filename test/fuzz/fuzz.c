/***********************************************************************************************************************************
Fuzz Target Driver
***********************************************************************************************************************************/
#include <build.h>

#include <stdio.h>
#include <stdlib.h>

#include "common/debug.h"
#include "common/error/error.h"
#include "common/macro.h"
#include "common/memContext.h"
#include "common/stackTrace.h"

#include "fuzz.h"

/***********************************************************************************************************************************
Fuzzing engine entry points
***********************************************************************************************************************************/
int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

/**********************************************************************************************************************************/
int
LLVMFuzzerInitialize(int *const argc, char ***const argv)
{
    FUNCTION_TEST_BEGIN();
        (void)argc;                                                 // Command line arguments are not used
        (void)argv;
    FUNCTION_TEST_END();

    // Set stack trace and mem context error cleanup handlers
    static const ErrorHandlerFunction errorHandlerList[] = {stackTraceClean, memContextClean};
    errorHandlerSet(errorHandlerList, LENGTH_OF(errorHandlerList));

    FUNCTION_TEST_RETURN(INT, 0);
}

/**********************************************************************************************************************************/
int
LLVMFuzzerTestOneInput(const uint8_t *const data, const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        TRY_BEGIN()
        {
            fuzzTarget(BUF(data, size));
        }
        CATCH_ANY()
        {
            // Errors thrown for invalid input are expected
        }
        CATCH_FATAL()
        {
            fprintf(stderr, "%s: %s\n%s\n", errorName(), errorMessage(), errorStackTrace());
            fflush(stderr);
            abort();
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(INT, 0);
}
