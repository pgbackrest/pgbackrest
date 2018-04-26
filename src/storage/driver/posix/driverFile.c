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
#include "storage/driver/posix/driverFile.h"

/***********************************************************************************************************************************
Open a file

Returns the handle of the open file, or -1 for reads if the file is missing and -1 for writes if the path is mssing.
***********************************************************************************************************************************/
int
storageFilePosixOpen(
    const String *name, int flags, mode_t mode, bool ignoreMissing, const ErrorType *errorType, const char *purpose)
{
    int result = -1;

    result = open(strPtr(name), flags, mode);

    if (result == -1)
    {
        if (errno != ENOENT || !ignoreMissing)
            THROWP_SYS_ERROR(errorType, "unable to open '%s' for %s", strPtr(name), purpose);
    }

    return result;
}

/***********************************************************************************************************************************
Sync a file/directory handle
***********************************************************************************************************************************/
void
storageFilePosixSync(int handle, const String *name, const ErrorType *errorType, bool closeOnError)
{
    if (fsync(handle) == -1)
    {
        int errNo = errno;

        // Close if requested but don't report errors -- we want to report the sync error instead
        if (closeOnError)
            close(handle);

        THROWP_SYS_ERROR_CODE(errNo, errorType, "unable to sync '%s'", strPtr(name));
    }
}

/***********************************************************************************************************************************
Close a file/directory handle
***********************************************************************************************************************************/
void
storageFilePosixClose(int handle, const String *name, const ErrorType *errorType)
{
    if (close(handle) == -1)
        THROWP_SYS_ERROR(errorType, "unable to close '%s'", strPtr(name));
}
