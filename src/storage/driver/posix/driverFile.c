/***********************************************************************************************************************************
Storage File Routines For Posix
***********************************************************************************************************************************/
// So fsync() will work on older glib versions
#ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200112L
#endif

#include <fcntl.h>
#include <unistd.h>

#include "common/assert.h"
#include "common/debug.h"
#include "storage/driver/posix/driverFile.h"

/***********************************************************************************************************************************
Open a file

Returns the handle of the open file, or -1 for reads if the file is missing and -1 for writes if the path is mssing.
***********************************************************************************************************************************/
int
storageFilePosixOpen(
    const String *name, int flags, mode_t mode, bool ignoreMissing, const ErrorType *errorType, const char *purpose)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(INT, flags);
        FUNCTION_TEST_PARAM(MODE, mode);
        FUNCTION_TEST_PARAM(BOOL, ignoreMissing);
        FUNCTION_TEST_PARAM(ERROR_TYPE, errorType);
        FUNCTION_TEST_PARAM(STRINGZ, purpose);

        FUNCTION_TEST_ASSERT(name != NULL);
        FUNCTION_TEST_ASSERT(errorType != NULL);
        FUNCTION_TEST_ASSERT(purpose != NULL);
    FUNCTION_TEST_END();

    int result = -1;

    result = open(strPtr(name), flags, mode);

    if (result == -1)
    {
        if (errno != ENOENT || !ignoreMissing)
            THROWP_SYS_ERROR_FMT(errorType, "unable to open '%s' for %s", strPtr(name), purpose);
    }

    FUNCTION_TEST_RESULT(INT, result);
}

/***********************************************************************************************************************************
Sync a file/directory handle
***********************************************************************************************************************************/
void
storageFilePosixSync(int handle, const String *name, const ErrorType *errorType, bool closeOnError)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, handle);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(ERROR_TYPE, errorType);
        FUNCTION_TEST_PARAM(BOOL, closeOnError);

        FUNCTION_TEST_ASSERT(handle != -1);
        FUNCTION_TEST_ASSERT(name != NULL);
        FUNCTION_TEST_ASSERT(errorType != NULL);
    FUNCTION_TEST_END();

    if (fsync(handle) == -1)
    {
        int errNo = errno;

        // Close if requested but don't report errors -- we want to report the sync error instead
        if (closeOnError)
            close(handle);

        THROWP_SYS_ERROR_CODE_FMT(errNo, errorType, "unable to sync '%s'", strPtr(name));
    }

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Close a file/directory handle
***********************************************************************************************************************************/
void
storageFilePosixClose(int handle, const String *name, const ErrorType *errorType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, handle);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(ERROR_TYPE, errorType);

        FUNCTION_TEST_ASSERT(handle != -1);
        FUNCTION_TEST_ASSERT(name != NULL);
        FUNCTION_TEST_ASSERT(errorType != NULL);
    FUNCTION_TEST_END();

    if (close(handle) == -1)
        THROWP_SYS_ERROR_FMT(errorType, "unable to close '%s'", strPtr(name));

    FUNCTION_TEST_RESULT_VOID();
}
