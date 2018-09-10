/***********************************************************************************************************************************
Storage File Read Driver For Posix
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/driver/posix/driverFile.h"
#include "storage/driver/posix/driverRead.h"

/***********************************************************************************************************************************
Storage file structure
***********************************************************************************************************************************/
struct StorageFileReadPosix
{
    MemContext *memContext;

    String *name;
    bool ignoreMissing;

    int handle;
    bool eof;
};

/***********************************************************************************************************************************
Create a new file
***********************************************************************************************************************************/
StorageFileReadPosix *
storageFileReadPosixNew(const String *name, bool ignoreMissing)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, name);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);

        FUNCTION_TEST_ASSERT(name != NULL);
    FUNCTION_DEBUG_END();

    StorageFileReadPosix *this = NULL;

    // Create the file object
    MEM_CONTEXT_NEW_BEGIN("StorageFileReadPosix")
    {
        this = memNew(sizeof(StorageFileReadPosix));
        this->memContext = MEM_CONTEXT_NEW();
        this->name = strDup(name);
        this->ignoreMissing = ignoreMissing;

        this->handle = -1;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(STORAGE_FILE_READ_POSIX, this);
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
bool
storageFileReadPosixOpen(StorageFileReadPosix *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->handle == -1);
    FUNCTION_DEBUG_END();

    bool result = false;

    // Open the file and handle errors
    this->handle = storageFilePosixOpen(this->name, O_RDONLY, 0, this->ignoreMissing, true, "read");

    // On success set free callback to ensure file handle is freed
    if (this->handle != -1)
    {
        memContextCallback(this->memContext, (MemContextCallback)storageFileReadPosixFree, this);
        result = true;
    }

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
size_t
storageFileReadPosix(StorageFileReadPosix *this, Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ_POSIX, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_DEBUG_ASSERT(this != NULL && this->handle != -1);
        FUNCTION_DEBUG_ASSERT(buffer != NULL && !bufFull(buffer));
    FUNCTION_DEBUG_END();

    // Read if EOF has not been reached
    ssize_t actualBytes = 0;

    if (!this->eof)
    {
        // Read and handle errors
        size_t expectedBytes = bufRemains(buffer);
        actualBytes = read(this->handle, bufRemainsPtr(buffer), expectedBytes);

        // Error occurred during write
        if (actualBytes == -1)
            THROW_SYS_ERROR_FMT(FileReadError, "unable to read '%s'", strPtr(this->name));

        // Update amount of buffer used
        bufUsedInc(buffer, (size_t)actualBytes);

        // If less data than expected was read then EOF.  The file may not actually be EOF but we are not concerned with files that
        // are growing.  Just read up to the point where the file is being extended.
        if ((size_t)actualBytes != expectedBytes)
            this->eof = true;
    }

    FUNCTION_DEBUG_RESULT(SIZE, (size_t)actualBytes);
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
void
storageFileReadPosixClose(StorageFileReadPosix *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    // Close if the file has not already been closed
    if (this->handle != -1)
    {
        // Close the file
        storageFilePosixClose(this->handle, this->name, true);

        this->handle = -1;
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
bool
storageFileReadPosixEof(StorageFileReadPosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->eof);
}

/***********************************************************************************************************************************
Should a missing file be ignored?
***********************************************************************************************************************************/
bool
storageFileReadPosixIgnoreMissing(StorageFileReadPosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->ignoreMissing);
}

/***********************************************************************************************************************************
File name
***********************************************************************************************************************************/
const String *
storageFileReadPosixName(StorageFileReadPosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->name);
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileReadPosixFree(StorageFileReadPosix *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ_POSIX, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
    {
        storageFileReadPosixClose(this);
        memContextFree(this->memContext);
    }

    FUNCTION_DEBUG_RESULT_VOID();
}
