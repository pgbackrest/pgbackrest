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
    IoRead *io;
};

/***********************************************************************************************************************************
Create a new storage file
***********************************************************************************************************************************/
StorageFileRead *
storageFileReadNew(const String *name, bool ignoreMissing)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, name);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);

        FUNCTION_TEST_ASSERT(name != NULL);
    FUNCTION_DEBUG_END();

    StorageFileRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageFileRead")
    {
        this = memNew(sizeof(StorageFileRead));
        this->memContext = memContextCurrent();

        this->fileDriver = storageFileReadPosixNew(name, ignoreMissing);

        this->io = ioReadNew(
            this->fileDriver, (IoReadOpen)storageFileReadPosixOpen, (IoReadProcess)storageFileReadPosix,
            (IoReadClose)storageFileReadPosixClose, (IoReadEof)storageFileReadPosixEof);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(STORAGE_FILE_READ, this);
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
Get file driver
***********************************************************************************************************************************/
StorageFileReadPosix *
storageFileReadDriver(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STORAGE_FILE_READ_POSIX, this->fileDriver);
}

/***********************************************************************************************************************************
Get io interface
***********************************************************************************************************************************/
IoRead *
storageFileReadIo(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_READ, this->io);
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
