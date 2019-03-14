/***********************************************************************************************************************************
CIFS Storage Driver
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "storage/driver/cifs/storage.h"
#include "storage/driver/posix/storage.h"

/***********************************************************************************************************************************
Driver type constant string
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_DRIVER_CIFS_TYPE_STR,                         STORAGE_DRIVER_CIFS_TYPE);

/***********************************************************************************************************************************
Object type

This type *must* stay in sync with StorageDriverPosix since it is cast to StorageDriverPosix.
***********************************************************************************************************************************/
struct StorageDriverCifs
{
    MemContext *memContext;                                         // Object memory context
    Storage *interface;                                             // Driver interface
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
StorageDriverCifs *
storageDriverCifsNew(
    const String *path, mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
    FUNCTION_LOG_END();

    ASSERT(path != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    // Create the object
    StorageDriverCifs *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageDriverCifs")
    {
        this = memNew(sizeof(StorageDriverCifs));
        this->memContext = MEM_CONTEXT_NEW();

        this->interface = storageNewP(
            STORAGE_DRIVER_CIFS_TYPE_STR, path, modeFile, modePath, write, pathExpressionFunction, this,
            .exists = (StorageInterfaceExists)storageDriverPosixExists, .info = (StorageInterfaceInfo)storageDriverPosixInfo,
            .list = (StorageInterfaceList)storageDriverPosixList, .move = (StorageInterfaceMove)storageDriverPosixMove,
            .newRead = (StorageInterfaceNewRead)storageDriverPosixNewRead,
            .newWrite = (StorageInterfaceNewWrite)storageDriverCifsNewWrite,
            .pathCreate = (StorageInterfacePathCreate)storageDriverPosixPathCreate,
            .pathRemove = (StorageInterfacePathRemove)storageDriverPosixPathRemove,
            .pathSync = (StorageInterfacePathSync)storageDriverCifsPathSync,
            .remove = (StorageInterfaceRemove)storageDriverPosixRemove);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_DRIVER_CIFS, this);
}

/***********************************************************************************************************************************
New file write object
***********************************************************************************************************************************/
StorageFileWrite *
storageDriverCifsNewWrite(
    StorageDriverCifs *this, const String *file, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile, bool syncPath,
    bool atomic)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_CIFS, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, createPath);
        FUNCTION_LOG_PARAM(BOOL, syncFile);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
        FUNCTION_LOG_PARAM(BOOL, atomic);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_FILE_WRITE,
        storageDriverPosixFileWriteInterface(
            storageDriverPosixFileWriteNew(
                (StorageDriverPosix *)this, file, modeFile, modePath, createPath, syncFile, false, atomic)));
}

/***********************************************************************************************************************************
Sync a path

CIFS does not support explicit syncs.
***********************************************************************************************************************************/
void
storageDriverCifsPathSync(StorageDriverCifs *this, const String *path, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_CIFS, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
Storage *
storageDriverCifsInterface(const StorageDriverCifs *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_CIFS, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface);
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageDriverCifsFree(StorageDriverCifs *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_CIFS, this);
    FUNCTION_LOG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_LOG_RETURN_VOID();
}
