/***********************************************************************************************************************************
Storage File Write Interface
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/fileWrite.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageFileWrite
{
    MemContext *memContext;
    const String *type;
    void *driver;
    StorageFileWriteInterface interface;
};

/***********************************************************************************************************************************
Create a new storage file

This object expects its context to be created in advance.  This is so the calling function can add whatever data it wants without
required multiple functions and contexts to make it safe.
***********************************************************************************************************************************/
StorageFileWrite *
storageFileWriteNew(const String *type, void *driver, StorageFileWriteInterface interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, type);
        FUNCTION_LOG_PARAM(VOIDP, driver);
        FUNCTION_LOG_PARAM(STORAGE_FILE_WRITE_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(type != NULL);
    ASSERT(driver != NULL);
    ASSERT(interface.atomic != NULL);
    ASSERT(interface.createPath != NULL);
    ASSERT(interface.io != NULL);
    ASSERT(interface.modeFile != NULL);
    ASSERT(interface.modePath != NULL);
    ASSERT(interface.name != NULL);
    ASSERT(interface.syncFile != NULL);
    ASSERT(interface.syncPath != NULL);

    StorageFileWrite *this = memNew(sizeof(StorageFileWrite));
    this->memContext = memContextCurrent();

    this->type = type;
    this->driver = driver;
    this->interface = interface;

    FUNCTION_LOG_RETURN(STORAGE_FILE_WRITE, this);
}

/***********************************************************************************************************************************
Move the file object to a new context
***********************************************************************************************************************************/
StorageFileWrite *
storageFileWriteMove(StorageFileWrite *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RETURN(STORAGE_FILE_WRITE, this);
}

/***********************************************************************************************************************************
Will the file be written atomically?

Atomic writes means the file will be complete or be missing.  Filesystems have different ways to accomplish this.
***********************************************************************************************************************************/
bool
storageFileWriteAtomic(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->interface.atomic(this->driver));
}

/***********************************************************************************************************************************
Will the path be created if required?
***********************************************************************************************************************************/
bool
storageFileWriteCreatePath(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->interface.createPath(this->driver));
}

/***********************************************************************************************************************************
Get file driver
***********************************************************************************************************************************/
void *
storageFileWriteFileDriver(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(VOIDP, this->driver);
}

/***********************************************************************************************************************************
Get the IO object
***********************************************************************************************************************************/
IoWrite *
storageFileWriteIo(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(IO_WRITE, this->interface.io(this->driver));
}

/***********************************************************************************************************************************
Get file mode
***********************************************************************************************************************************/
mode_t
storageFileWriteModeFile(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(MODE, this->interface.modeFile(this->driver));
}

/***********************************************************************************************************************************
Get path mode
***********************************************************************************************************************************/
mode_t
storageFileWriteModePath(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(MODE, this->interface.modePath(this->driver));
}

/***********************************************************************************************************************************
Get file name
***********************************************************************************************************************************/
const String *
storageFileWriteName(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(CONST_STRING, this->interface.name(this->driver));
}

/***********************************************************************************************************************************
Will the file be synced after it is closed?
***********************************************************************************************************************************/
bool
storageFileWriteSyncFile(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->interface.syncFile(this->driver));
}

/***********************************************************************************************************************************
Will the path be synced after the file is closed?
***********************************************************************************************************************************/
bool
storageFileWriteSyncPath(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->interface.syncPath(this->driver));
}

/***********************************************************************************************************************************
Get file type
***********************************************************************************************************************************/
const String *
storageFileWriteType(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(CONST_STRING, this->type);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
storageFileWriteToLog(const StorageFileWrite *this)
{
    return strNewFmt(
        "{type: %s, name: %s, modeFile: %04o, modePath: %04o, createPath: %s, syncFile: %s, syncPath: %s, atomic: %s}",
        strPtr(this->type), strPtr(strToLog(storageFileWriteName(this))), storageFileWriteModeFile(this),
        storageFileWriteModePath(this), cvtBoolToConstZ(storageFileWriteCreatePath(this)),
        cvtBoolToConstZ(storageFileWriteSyncFile(this)), cvtBoolToConstZ(storageFileWriteSyncPath(this)),
        cvtBoolToConstZ(storageFileWriteAtomic(this)));
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileWriteFree(const StorageFileWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_LOG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_LOG_RETURN_VOID();
}
