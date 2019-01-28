/***********************************************************************************************************************************
Posix Common File Routines
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/debug.h"
#include "storage/driver/posix/common.h"

/***********************************************************************************************************************************
Open a file

Returns the handle of the open file, or -1 for reads if the file is missing and -1 for writes if the path is mssing.
***********************************************************************************************************************************/
int
storageDriverPosixFileOpen(
    const String *name, int flags, mode_t mode, bool ignoreMissing, bool file, const char *purpose)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(INT, flags);
        FUNCTION_TEST_PARAM(MODE, mode);
        FUNCTION_TEST_PARAM(BOOL, ignoreMissing);
        FUNCTION_TEST_PARAM(BOOL, file);                            //  Is this a file or a path?
        FUNCTION_TEST_PARAM(STRINGZ, purpose);
    FUNCTION_TEST_END();

    ASSERT(name != NULL);
    ASSERT(purpose != NULL);

    int result = -1;

    result = open(strPtr(name), flags, mode);

    if (result == -1)
    {
        if (errno != ENOENT || !ignoreMissing)
        {
            THROWP_SYS_ERROR_FMT(
                errno == ENOENT ? (file ? &FileMissingError : &PathMissingError) : (file ? &FileOpenError : &PathOpenError),
                "unable to open '%s' for %s", strPtr(name), purpose);
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Sync a file/directory handle
***********************************************************************************************************************************/
void
storageDriverPosixFileSync(int handle, const String *name, bool file, bool closeOnError)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, handle);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(BOOL, file);                            //  Is this a file or a path?
        FUNCTION_TEST_PARAM(BOOL, closeOnError);
    FUNCTION_TEST_END();

    ASSERT(handle != -1);
    ASSERT(name != NULL);

    if (fsync(handle) == -1)
    {
        int errNo = errno;

        // Close if requested but don't report errors -- we want to report the sync error instead
        if (closeOnError)
            close(handle);

        THROWP_SYS_ERROR_CODE_FMT(errNo, file ? &FileSyncError : &PathSyncError, "unable to sync '%s'", strPtr(name));
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Close a file/directory handle
***********************************************************************************************************************************/
void
storageDriverPosixFileClose(int handle, const String *name, bool file)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, handle);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(BOOL, file);                            //  Is this a file or a path?
    FUNCTION_TEST_END();

    ASSERT(handle != -1);
    ASSERT(name != NULL);

    if (close(handle) == -1)
        THROWP_SYS_ERROR_FMT(file ? &FileCloseError : &PathCloseError, "unable to close '%s'", strPtr(name));

    FUNCTION_TEST_RETURN_VOID();
}
