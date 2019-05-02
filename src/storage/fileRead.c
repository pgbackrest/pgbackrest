/***********************************************************************************************************************************
Storage File Read Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/fileRead.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageFileRead
{
    MemContext *memContext;                                         // Object mem context
    void *driver;
    const StorageFileReadInterface *interface;
    IoRead *io;
};

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_FILE_READ_INTERFACE_TYPE                                                                              \
    StorageFileReadInterface
#define FUNCTION_LOG_STORAGE_FILE_READ_INTERFACE_FORMAT(value, buffer, bufferSize)                                                 \
    objToLog(&value, "StorageFileReadInterface", buffer, bufferSize)

/***********************************************************************************************************************************
Create a new storage file
***********************************************************************************************************************************/
StorageFileRead *
storageFileReadNew(void *driver, const StorageFileReadInterface *interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM_P(STORAGE_FILE_READ_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);

    StorageFileRead *this = NULL;

    this = memNew(sizeof(StorageFileRead));
    this->memContext = memContextCurrent();
    this->driver = driver;
    this->interface = interface;

    this->io = ioReadNew(driver, interface->ioInterface);

    FUNCTION_LOG_RETURN(STORAGE_FILE_READ, this);
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
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Get file driver
***********************************************************************************************************************************/
void *
storageFileReadDriver(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->driver);
}

/***********************************************************************************************************************************
Should a missing file be ignored?
***********************************************************************************************************************************/
bool
storageFileReadIgnoreMissing(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->ignoreMissing);
}

/***********************************************************************************************************************************
Get io interface
***********************************************************************************************************************************/
IoRead *
storageFileReadIo(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->io);
}

/***********************************************************************************************************************************
Get file name
***********************************************************************************************************************************/
const String *
storageFileReadName(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->name);
}

/***********************************************************************************************************************************
Get file type
***********************************************************************************************************************************/
const String *
storageFileReadType(const StorageFileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->type);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
storageFileReadToLog(const StorageFileRead *this)
{
    return strNewFmt(
        "{type: %s, name: %s, ignoreMissing: %s}", strPtr(this->interface->type), strPtr(strToLog(this->interface->name)),
        cvtBoolToConstZ(this->interface->ignoreMissing));
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileReadFree(const StorageFileRead *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_FILE_READ, this);
    FUNCTION_LOG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_LOG_RETURN_VOID();
}
