/***********************************************************************************************************************************
Harness for Stack Trace Testing
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/harnessDebug.h"
#include "common/harnessStackTrace.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Shim install state
***********************************************************************************************************************************/
#ifdef HAVE_LIBBACKTRACE

static struct
{
    bool backTraceShimInstalled;
} hrnStackTraceStatic;

/***********************************************************************************************************************************
Shim to return true on backtrace callback indicating that there is no backtrace
***********************************************************************************************************************************/
static int
stackTraceBackCallback(
    void *const dataVoid, const uintptr_t pc, const char *fileName, const int fileLine, const char *const functionName)
{
    if (hrnStackTraceStatic.backTraceShimInstalled)
        return true;

    return stackTraceBackCallback_SHIMMED(dataVoid, pc, fileName, fileLine, functionName);
}

/**********************************************************************************************************************************/
void
hrnStackTraceBackShimInstall(void)
{
    FUNCTION_HARNESS_VOID();

    hrnStackTraceStatic.backTraceShimInstalled = true;

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnStackTraceBackShimUninstall(void)
{
    FUNCTION_HARNESS_VOID();

    hrnStackTraceStatic.backTraceShimInstalled = false;

    FUNCTION_HARNESS_RETURN_VOID();
}

#endif // HAVE_LIBBACKTRACE
