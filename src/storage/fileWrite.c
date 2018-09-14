/***********************************************************************************************************************************
Storage File Write Interface
***********************************************************************************************************************************/
#include "common/assert.h"
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
    const StorageFileWriteInterface *interface;
};

/***********************************************************************************************************************************
Create a new storage file

This object expects its context to be created in advance.  This is so the calling function can add whatever data it wants without
required multiple functions and contexts to make it safe.
***********************************************************************************************************************************/
StorageFileWrite *
storageFileWriteNew(const String *type, void *driver, const StorageFileWriteInterface *interface)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, type);
        FUNCTION_DEBUG_PARAM(VOIDP, driver);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_WRITE_INTERFACE, interface);

        FUNCTION_TEST_ASSERT(type != NULL);
        FUNCTION_TEST_ASSERT(driver != NULL);
        FUNCTION_TEST_ASSERT(interface != NULL);
        FUNCTION_TEST_ASSERT(interface->atomic != NULL);
        FUNCTION_TEST_ASSERT(interface->createPath != NULL);
        FUNCTION_TEST_ASSERT(interface->io != NULL);
        FUNCTION_TEST_ASSERT(interface->modeFile != NULL);
        FUNCTION_TEST_ASSERT(interface->modePath != NULL);
        FUNCTION_TEST_ASSERT(interface->name != NULL);
        FUNCTION_TEST_ASSERT(interface->syncFile != NULL);
        FUNCTION_TEST_ASSERT(interface->syncPath != NULL);
    FUNCTION_DEBUG_END();

    StorageFileWrite *this = memNew(sizeof(StorageFileWrite));
    this->memContext = memContextCurrent();

    this->type = type;
    this->driver = driver;
    this->interface = interface;

    FUNCTION_DEBUG_RESULT(STORAGE_FILE_WRITE, this);
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

        FUNCTION_TEST_ASSERT(parentNew != NULL);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RESULT(STORAGE_FILE_WRITE, this);
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

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->interface->atomic(this->driver));
}

/***********************************************************************************************************************************
Will the path be created if required?
***********************************************************************************************************************************/
bool
storageFileWriteCreatePath(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->interface->createPath(this->driver));
}

/***********************************************************************************************************************************
Get file driver
***********************************************************************************************************************************/
void *
storageFileWriteFileDriver(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(VOIDP, this->driver);
}

/***********************************************************************************************************************************
Get the IO object
***********************************************************************************************************************************/
IoWrite *
storageFileWriteIo(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_WRITE, this->interface->io(this->driver));
}

/***********************************************************************************************************************************
Get file mode
***********************************************************************************************************************************/
mode_t
storageFileWriteModeFile(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(MODE, this->interface->modeFile(this->driver));
}

/***********************************************************************************************************************************
Get path mode
***********************************************************************************************************************************/
mode_t
storageFileWriteModePath(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(MODE, this->interface->modePath(this->driver));
}

/***********************************************************************************************************************************
Get file name
***********************************************************************************************************************************/
const String *
storageFileWriteName(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->interface->name(this->driver));
}

/***********************************************************************************************************************************
Will the file be synced after it is closed?
***********************************************************************************************************************************/
bool
storageFileWriteSyncFile(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->interface->syncFile(this->driver));
}

/***********************************************************************************************************************************
Will the path be synced after the file is closed?
***********************************************************************************************************************************/
bool
storageFileWriteSyncPath(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->interface->syncPath(this->driver));
}

/***********************************************************************************************************************************
Get file type
***********************************************************************************************************************************/
const String *
storageFileWriteType(const StorageFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->type);
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
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_WRITE, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
