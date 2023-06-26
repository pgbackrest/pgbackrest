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
    // Local process shims
    bool localShimFdReady;
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

    bool result = true;

    if (hrnFdStatic.localShimFdReady)
    {
        // When shim is installed return false if read and write are both passed in as true otherwise return true
        // TBD: Determine if we can set properties on fd such that we can use poll in fdReady as intended
        if (read && write)
            result = false;
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
