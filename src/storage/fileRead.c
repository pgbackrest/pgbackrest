/***********************************************************************************************************************************
Storage File Read Interface
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/fileRead.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageFileRead
{
    MemContext *memContext;
    const String *type;
    const StorageFileReadInterface *interface;
    void *driver;

    StorageFileReadIgnoreMissing ignoreMissing;
    StorageFileReadIo io;
    StorageFileReadName name;
};

/***********************************************************************************************************************************
Create a new storage file
***********************************************************************************************************************************/
StorageFileRead *
storageFileReadNew(const String *type, void *driver, const StorageFileReadInterface *interface)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, type);
        FUNCTION_DEBUG_PARAM(VOIDP, driver);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ_INTERFACE, interface);

        FUNCTION_TEST_ASSERT(type != NULL);
        FUNCTION_TEST_ASSERT(driver != NULL);
        FUNCTION_TEST_ASSERT(interface->ignoreMissing != NULL);
        FUNCTION_TEST_ASSERT(interface->io != NULL);
        FUNCTION_TEST_ASSERT(interface->name != NULL);
    FUNCTION_DEBUG_END();

    StorageFileRead *this = NULL;

    this = memNew(sizeof(StorageFileRead));
    this->memContext = memContextCurrent();
    this->type = type;
    this->interface = interface;
    this->driver = driver;

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
void *
storageFileReadDriver(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(VOIDP, this->driver);
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

    FUNCTION_TEST_RESULT(BOOL, this->interface->ignoreMissing(this->driver));
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

    FUNCTION_TEST_RESULT(IO_READ, this->interface->io(this->driver));
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

    FUNCTION_TEST_RESULT(CONST_STRING, this->interface->name(this->driver));
}

/***********************************************************************************************************************************
Get file type
***********************************************************************************************************************************/
const String *
storageFileReadType(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->type);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
storageFileReadToLog(const StorageFileRead *this)
{
    return strNewFmt(
        "{type: %s, name: %s, ignoreMissing: %s}", strPtr(this->type), strPtr(strToLog(storageFileReadName(this))),
        cvtBoolToConstZ(storageFileReadIgnoreMissing(this)));
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileReadFree(const StorageFileRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
