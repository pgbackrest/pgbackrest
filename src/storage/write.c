/***********************************************************************************************************************************
Storage Write Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "storage/write.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageWrite
{
    MemContext *memContext;                                         // Object mem context
    void *driver;
    const StorageWriteInterface *interface;
    IoWrite *io;
};

OBJECT_DEFINE_MOVE(STORAGE_WRITE);
OBJECT_DEFINE_FREE(STORAGE_WRITE);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_INTERFACE_TYPE                                                                                  \
    StorageWriteInterface
#define FUNCTION_LOG_STORAGE_WRITE_INTERFACE_FORMAT(value, buffer, bufferSize)                                                     \
    objToLog(&value, "StorageWriteInterface", buffer, bufferSize)

/***********************************************************************************************************************************
Create a new storage file

This object expects its context to be created in advance.  This is so the calling function can add whatever data it wants without
required multiple functions and contexts to make it safe.
***********************************************************************************************************************************/
StorageWrite *
storageWriteNew(void *driver, const StorageWriteInterface *interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM_P(STORAGE_WRITE_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);

    StorageWrite *this = memNew(sizeof(StorageWrite));

    *this = (StorageWrite)
    {
        .memContext = memContextCurrent(),
        .driver = driver,
        .interface = interface,
        .io = ioWriteNew(driver, interface->ioInterface),
    };

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}

/***********************************************************************************************************************************
Will the file be written atomically?

Atomic writes means the file will be complete or be missing.  Filesystems have different ways to accomplish this.
***********************************************************************************************************************************/
bool
storageWriteAtomic(const StorageWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->atomic);
}

/***********************************************************************************************************************************
Will the path be created if required?
***********************************************************************************************************************************/
bool
storageWriteCreatePath(const StorageWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->createPath);
}

/***********************************************************************************************************************************
Get file driver
***********************************************************************************************************************************/
void *
storageWriteDriver(const StorageWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->driver);
}

/***********************************************************************************************************************************
Get the IO object
***********************************************************************************************************************************/
IoWrite *
storageWriteIo(const StorageWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->io);
}

/***********************************************************************************************************************************
Get file mode
***********************************************************************************************************************************/
mode_t
storageWriteModeFile(const StorageWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->modeFile);
}

/***********************************************************************************************************************************
Get path mode
***********************************************************************************************************************************/
mode_t
storageWriteModePath(const StorageWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->modePath);
}

/***********************************************************************************************************************************
Get file name
***********************************************************************************************************************************/
const String *
storageWriteName(const StorageWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->name);
}

/***********************************************************************************************************************************
Will the file be synced after it is closed?
***********************************************************************************************************************************/
bool
storageWriteSyncFile(const StorageWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->syncFile);
}

/***********************************************************************************************************************************
Will the path be synced after the file is closed?
***********************************************************************************************************************************/
bool
storageWriteSyncPath(const StorageWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->syncPath);
}

/***********************************************************************************************************************************
Get file type
***********************************************************************************************************************************/
const String *
storageWriteType(const StorageWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->type);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
storageWriteToLog(const StorageWrite *this)
{
    return strNewFmt(
        "{type: %s, name: %s, modeFile: %04o, modePath: %04o, createPath: %s, syncFile: %s, syncPath: %s, atomic: %s}",
        strPtr(storageWriteType(this)), strPtr(strToLog(storageWriteName(this))), storageWriteModeFile(this),
        storageWriteModePath(this), cvtBoolToConstZ(storageWriteCreatePath(this)), cvtBoolToConstZ(storageWriteSyncFile(this)),
        cvtBoolToConstZ(storageWriteSyncPath(this)), cvtBoolToConstZ(storageWriteAtomic(this)));
}
