/***********************************************************************************************************************************
Harness for Fd Testing
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/harnessConfig.h"
#include "common/harnessDebug.h"
#include "common/harnessFd.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Shim install state
***********************************************************************************************************************************/
static struct
{
    bool localShimFdReady;                                          // Is shim installed?
    bool localShimFdReadyOne;                                       // Should the shim run once?
    bool localShimFdReadyOneResult;                                 // Shim result for single run
} hrnFdStatic;

/***********************************************************************************************************************************
Shim fdReady()
***********************************************************************************************************************************/
bool
fdReady(int fd, bool read, bool write, TimeMSec timeout)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(INT, fd);
        FUNCTION_HARNESS_PARAM(BOOL, read);
        FUNCTION_HARNESS_PARAM(BOOL, write);
        FUNCTION_HARNESS_PARAM(TIME_MSEC, timeout);
    FUNCTION_HARNESS_END();

    bool result;

    // If shim will run once then return the requested result
    if (hrnFdStatic.localShimFdReadyOne)
    {
        hrnFdStatic.localShimFdReadyOne = false;
        result = hrnFdStatic.localShimFdReadyOneResult;

        // If result is false sleep the full timeout
        if (!result)
            sleepMSec(timeout);
    }
    // Else if shim is installed return false if read and write are both true, otherwise return true
    else if (hrnFdStatic.localShimFdReady)
    {
        result = !(read && write);
    }
    // Else call normal function
    else
        result = fdReady_SHIMMED(fd, read, write, timeout);

    FUNCTION_HARNESS_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
void
hrnFdReadyShimInstall(void)
{
    FUNCTION_HARNESS_VOID();

    hrnFdStatic.localShimFdReady = true;

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnFdReadyShimUninstall(void)
{
    FUNCTION_HARNESS_VOID();

    hrnFdStatic.localShimFdReady = false;

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnFdReadyShimOne(const bool result)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BOOL, result);
    FUNCTION_HARNESS_END();

    hrnFdStatic.localShimFdReadyOne = true;
    hrnFdStatic.localShimFdReadyOneResult = result;

    FUNCTION_HARNESS_RETURN_VOID();
}
