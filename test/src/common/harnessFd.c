/***********************************************************************************************************************************
Harness for Fd Testing
***********************************************************************************************************************************/
#include <build.h>

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
    bool localShimFdReady;                                          // Is fdReady shim installed?
    bool localShimFdReadyOne;                                       // Should the fdReady shim run once?
    bool localShimFdReadyOneResult;                                 // Shim result for single run

    bool localShimIoFdWriteInternalOne;                             // Should ioFdWriteInternal shim run once?
    ssize_t localShimIoFdWriteInternalOneResult;                    // Return value for single run
    int localShimIoFdWriteInternalOneErrNo;                         // errno value for single run
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

/***********************************************************************************************************************************
Shim ioFdWriteInternal()
***********************************************************************************************************************************/
static ssize_t
ioFdWriteInternal(const int fd, const void *const buffer, const size_t size)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(INT, fd);
        FUNCTION_HARNESS_PARAM_P(VOID, buffer);
        FUNCTION_HARNESS_PARAM(SIZE, size);
    FUNCTION_HARNESS_END();

    ssize_t result;

    // If shim will run once then return the requested result
    if (hrnFdStatic.localShimIoFdWriteInternalOne)
    {
        hrnFdStatic.localShimIoFdWriteInternalOne = false;
        result = hrnFdStatic.localShimIoFdWriteInternalOneResult;

        // Set errno if result is -1
        if (result == -1)
            errno = hrnFdStatic.localShimIoFdWriteInternalOneErrNo;
    }
    // Else call normal function
    else
        result = ioFdWriteInternal_SHIMMED(fd, buffer, size);

    FUNCTION_HARNESS_RETURN(SSIZE, result);
}

/**********************************************************************************************************************************/
void
hrnIoFdWriteInternalShimOne(const ssize_t result, const int errNo)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(INT64, result);
        FUNCTION_HARNESS_PARAM(INT, errNo);
    FUNCTION_HARNESS_END();

    hrnFdStatic.localShimIoFdWriteInternalOne = true;
    hrnFdStatic.localShimIoFdWriteInternalOneResult = result;
    hrnFdStatic.localShimIoFdWriteInternalOneErrNo = errNo;

    FUNCTION_HARNESS_RETURN_VOID();
}
