/***********************************************************************************************************************************
Storage File Read
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/fileRead.h"

/***********************************************************************************************************************************
Storage file structure
***********************************************************************************************************************************/
struct StorageFileRead
{
    MemContext *memContext;
    StorageFileReadPosix *fileDriver;
    size_t bufferSize;
};

/***********************************************************************************************************************************
Create a new storage file
***********************************************************************************************************************************/
StorageFileRead *
storageFileReadNew(const String *name, bool ignoreMissing, size_t bufferSize)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, name);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);
        FUNCTION_DEBUG_PARAM(BOOL, bufferSize);

        FUNCTION_TEST_ASSERT(name != NULL);
        FUNCTION_TEST_ASSERT(bufferSize > 0);
    FUNCTION_DEBUG_END();

    StorageFileRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageFileRead")
    {
        this = memNew(sizeof(StorageFileRead));
        this->memContext = memContextCurrent();

        this->fileDriver = storageFileReadPosixNew(name, ignoreMissing);
        this->bufferSize = bufferSize;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(STORAGE_FILE_READ, this);
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
bool
storageFileReadOpen(StorageFileRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(BOOL, storageFileReadPosixOpen(this->fileDriver));
}

/***********************************************************************************************************************************
Read data from the file
***********************************************************************************************************************************/
void
storageFileRead(StorageFileRead *this, Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    storageFileReadPosix(this->fileDriver, buffer);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Move the file object to a new context
***********************************************************************************************************************************/
StorageFileRead *
storageFileReadMove(StorageFileRead *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);

        FUNCTION_TEST_ASSERT(parentNew != NULL);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RESULT(STORAGE_FILE_READ, this);
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
void
storageFileReadClose(StorageFileRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    storageFileReadPosixClose(this->fileDriver);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Get buffer size
***********************************************************************************************************************************/
size_t
storageFileReadBufferSize(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(SIZE, this->bufferSize);
}

/***********************************************************************************************************************************
Get file driver
***********************************************************************************************************************************/
StorageFileReadPosix *
storageFileReadFileDriver(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STORAGE_FILE_READ_POSIX, this->fileDriver);
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
bool
storageFileReadEof(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, storageFileReadPosixEof(this->fileDriver));
}

/***********************************************************************************************************************************
Should a missing file be ignored?
***********************************************************************************************************************************/
bool
storageFileReadIgnoreMissing(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, storageFileReadPosixIgnoreMissing(this->fileDriver));
}

/***********************************************************************************************************************************
Get file name
***********************************************************************************************************************************/
const String *
storageFileReadName(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, storageFileReadPosixName(this->fileDriver));
}

/***********************************************************************************************************************************
Get file size
***********************************************************************************************************************************/
size_t
storageFileReadSize(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(SIZE, storageFileReadPosixSize(this->fileDriver));
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileReadFree(StorageFileRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
