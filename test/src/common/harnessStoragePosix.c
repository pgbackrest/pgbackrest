/***********************************************************************************************************************************
Posix Storage Test Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/harnessDebug.h"
#include "common/harnessStoragePosix.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Shim install state
***********************************************************************************************************************************/
static struct dirent hrnStoragePosixDirentBuf;

static struct
{
    unsigned int zeroCookieAtEntry;     // 1-indexed readdir call at which to inject d_off == 0 (0 = disabled)
    unsigned int zeroCookieRemaining;   // remaining injections
    unsigned int currentEntry;          // readdir calls in current scan
} hrnStoragePosixLocal;

/***********************************************************************************************************************************
Shim for storagePosixReadDir — injects a zero d_off at the configured position to simulate NFS cookie invalidation
***********************************************************************************************************************************/
static struct dirent *
storagePosixReadDir(DIR *const dir)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM_P(VOID, dir);
    FUNCTION_HARNESS_END();

    struct dirent *result = storagePosixReadDir_SHIMMED(dir);

    if (result == NULL)
    {
        hrnStoragePosixLocal.currentEntry = 0;
    }
    else if (hrnStoragePosixLocal.zeroCookieAtEntry != 0 && hrnStoragePosixLocal.zeroCookieRemaining > 0)
    {
        hrnStoragePosixLocal.currentEntry++;

        if (hrnStoragePosixLocal.currentEntry == hrnStoragePosixLocal.zeroCookieAtEntry)
        {
            hrnStoragePosixLocal.currentEntry = 0;
            hrnStoragePosixLocal.zeroCookieRemaining--;

            memcpy(&hrnStoragePosixDirentBuf, result, sizeof(hrnStoragePosixDirentBuf));
            hrnStoragePosixDirentBuf.d_off = 0;
            result = &hrnStoragePosixDirentBuf;
        }
    }

    FUNCTION_HARNESS_RETURN(VOID, result);
}

/***********************************************************************************************************************************
Configure zero-cookie injection
***********************************************************************************************************************************/
void
hrnStoragePosixReadDirZeroCookieSet(const unsigned int atEntry, const unsigned int count)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, atEntry);
        FUNCTION_HARNESS_PARAM(UINT, count);
    FUNCTION_HARNESS_END();

    ASSERT(atEntry > 0);
    ASSERT(count > 0);

    hrnStoragePosixLocal.zeroCookieAtEntry = atEntry;
    hrnStoragePosixLocal.zeroCookieRemaining = count;
    hrnStoragePosixLocal.currentEntry = 0;

    FUNCTION_HARNESS_RETURN_VOID();
}
