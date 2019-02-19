/***********************************************************************************************************************************
Remote Storage Driver
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "protocol/client.h"
#include "protocol/helper.h"
#include "storage/driver/remote/fileRead.h"
#include "storage/driver/remote/protocol.h"
#include "storage/driver/remote/storage.h"

/***********************************************************************************************************************************
Driver type constant string
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_DRIVER_REMOTE_TYPE_STR,                           STORAGE_DRIVER_REMOTE_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageDriverRemote
{
    MemContext *memContext;
    Storage *interface;                                             // Driver interface
    ProtocolClient *client;                                         // Protocol client
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
StorageDriverRemote *
storageDriverRemoteNew(
    const String *path, mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction,
    RemoteType remoteType, unsigned int remoteId)
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
    StorageDriverRemote *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageDriverRemote")
    {
        this = memNew(sizeof(StorageDriverRemote));
        this->memContext = MEM_CONTEXT_NEW();

        this->client = protocolGet(remoteType, remoteId);

        // Create the storage interface
        this->interface = storageNewP(
            STORAGE_DRIVER_REMOTE_TYPE_STR, path, modeFile, modePath, write, false, pathExpressionFunction, this,
            .exists = (StorageInterfaceExists)storageDriverRemoteExists, .info = (StorageInterfaceInfo)storageDriverRemoteInfo,
            .list = (StorageInterfaceList)storageDriverRemoteList, .newRead = (StorageInterfaceNewRead)storageDriverRemoteNewRead,
            .newWrite = (StorageInterfaceNewWrite)storageDriverRemoteNewWrite,
            .pathCreate = (StorageInterfacePathCreate)storageDriverRemotePathCreate,
            .pathRemove = (StorageInterfacePathRemove)storageDriverRemotePathRemove,
            .pathSync = (StorageInterfacePathSync)storageDriverRemotePathSync,
            .remove = (StorageInterfaceRemove)storageDriverRemoteRemove);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_DRIVER_REMOTE, this);
}

/***********************************************************************************************************************************
Does a file/path exist?
***********************************************************************************************************************************/
bool
storageDriverRemoteExists(StorageDriverRemote *this, const String *path)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = false;

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
File/path info
***********************************************************************************************************************************/
StorageInfo
storageDriverRemoteInfo(StorageDriverRemote *this, const String *file, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN(STORAGE_INFO, (StorageInfo){0});
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
StringList *
storageDriverRemoteList(StorageDriverRemote *this, const String *path, bool errorOnMissing, const String *expression)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
        FUNCTION_LOG_PARAM(STRING, expression);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!errorOnMissing);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Add parameters
        Variant *param = varNewVarLst(varLstNew());
        varLstAdd(varVarLst(param), varNewStr(path));
        varLstAdd(varVarLst(param), varNewBool(errorOnMissing));
        varLstAdd(varVarLst(param), varNewStr(expression));

        // Construct command
        KeyValue *command = kvPut(kvNew(), varNewStr(PROTOCOL_COMMAND_STR), varNewStr(PROTOCOL_COMMAND_STORAGE_LIST_STR));
        kvPut(command, varNewStr(PROTOCOL_PARAMETER_STR), param);

        result = strLstMove(strLstNewVarLst(varVarLst(protocolClientExecute(this->client, command, true))), MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
New file read object
***********************************************************************************************************************************/
StorageFileRead *
storageDriverRemoteNewRead(StorageDriverRemote *this, const String *file, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_FILE_READ,
        storageDriverRemoteFileReadInterface(storageDriverRemoteFileReadNew(this, this->client, file, ignoreMissing)));
}

/***********************************************************************************************************************************
New file write object
***********************************************************************************************************************************/
StorageFileWrite *
storageDriverRemoteNewWrite(
    StorageDriverRemote *this, const String *file, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile,
    bool syncPath, bool atomic)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, this);
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

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN(STORAGE_FILE_WRITE, NULL);
}

/***********************************************************************************************************************************
Create a path.  There are no physical paths on S3 so just return success.
***********************************************************************************************************************************/
void
storageDriverRemotePathCreate(StorageDriverRemote *this, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnExists);
        FUNCTION_LOG_PARAM(BOOL, noParentCreate);
        FUNCTION_LOG_PARAM(MODE, mode);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Remove a path
***********************************************************************************************************************************/
void
storageDriverRemotePathRemove(StorageDriverRemote *this, const String *path, bool errorOnMissing, bool recurse)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
        FUNCTION_LOG_PARAM(BOOL, recurse);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Sync a path.  There's no need for this on S3 so just return success.
***********************************************************************************************************************************/
void
storageDriverRemotePathSync(StorageDriverRemote *this, const String *path, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
void
storageDriverRemoteRemove(StorageDriverRemote *this, const String *file, bool errorOnMissing)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get storage interface
***********************************************************************************************************************************/
Storage *
storageDriverRemoteInterface(const StorageDriverRemote *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface);
}
