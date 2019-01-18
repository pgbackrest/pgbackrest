/***********************************************************************************************************************************
Remote Storage Driver
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "protocol/client.h"
#include "protocol/helper.h"
#include "storage/driver/remote/fileRead.h"
#include "storage/driver/remote/storage.h"

/***********************************************************************************************************************************
Driver type constant string
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_DRIVER_REMOTE_TYPE_STR,                           STORAGE_DRIVER_REMOTE_TYPE);

/***********************************************************************************************************************************
Command constants
***********************************************************************************************************************************/
STRING_STATIC(STORAGE_REMOTE_COMMAND_LIST_STR,                          "storageList");
STRING_STATIC(STORAGE_REMOTE_COMMAND_LIST_EXPRESSION_STR,               "strExpression");
STRING_STATIC(STORAGE_REMOTE_COMMAND_LIST_IGNORE_MISSING_STR,           "bIgnoreMissing");

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
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, path);
        FUNCTION_DEBUG_PARAM(MODE, modeFile);
        FUNCTION_DEBUG_PARAM(MODE, modePath);
        FUNCTION_DEBUG_PARAM(BOOL, write);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, pathExpressionFunction);

        FUNCTION_TEST_ASSERT(path != NULL);
        FUNCTION_TEST_ASSERT(modeFile != 0);
        FUNCTION_TEST_ASSERT(modePath != 0);
    FUNCTION_DEBUG_END();

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

    FUNCTION_DEBUG_RESULT(STORAGE_DRIVER_REMOTE, this);
}

/***********************************************************************************************************************************
Does a file/path exist?
***********************************************************************************************************************************/
bool
storageDriverRemoteExists(StorageDriverRemote *this, const String *path)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_DEBUG_PARAM(STRING, path);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(path != NULL);
    FUNCTION_DEBUG_END();

    bool result = false;

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
File/path info
***********************************************************************************************************************************/
StorageInfo
storageDriverRemoteInfo(StorageDriverRemote *this, const String *file, bool ignoreMissing)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_DEBUG_PARAM(STRING, file);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(file != NULL);
    FUNCTION_DEBUG_END();

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_DEBUG_RESULT(STORAGE_INFO, (StorageInfo){0});
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
StringList *
storageDriverRemoteList(StorageDriverRemote *this, const String *path, bool errorOnMissing, const String *expression)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_DEBUG_PARAM(STRING, path);
        FUNCTION_DEBUG_PARAM(BOOL, errorOnMissing);
        FUNCTION_DEBUG_PARAM(STRING, expression);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(!errorOnMissing);
    FUNCTION_DEBUG_END();

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Add optional parameters
        Variant *paramOpt = varNewKv();
        kvPut(varKv(paramOpt), varNewStr(STORAGE_REMOTE_COMMAND_LIST_IGNORE_MISSING_STR), varNewBool(!errorOnMissing));
        kvPut(varKv(paramOpt), varNewStr(STORAGE_REMOTE_COMMAND_LIST_EXPRESSION_STR), varNewStr(expression));

        // Add parameters
        Variant *param = varNewVarLst(varLstNew());
        varLstAdd(varVarLst(param), varNewStr(path));
        varLstAdd(varVarLst(param), paramOpt);

        // Construct command
        KeyValue *command = kvPut(kvNew(), varNewStr(PROTOCOL_COMMAND_STR), varNewStr(STORAGE_REMOTE_COMMAND_LIST_STR));
        kvPut(command, varNewStr(PROTOCOL_PARAMETER_STR), param);

        result = strLstMove(strLstNewVarLst(protocolClientExecute(this->client, command, true)), MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STRING_LIST, result);
}

/***********************************************************************************************************************************
New file read object
***********************************************************************************************************************************/
StorageFileRead *
storageDriverRemoteNewRead(StorageDriverRemote *this, const String *file, bool ignoreMissing)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_DEBUG_PARAM(STRING, file);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(
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
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_DEBUG_PARAM(STRING, file);
        FUNCTION_DEBUG_PARAM(MODE, modeFile);
        FUNCTION_DEBUG_PARAM(MODE, modePath);
        FUNCTION_DEBUG_PARAM(BOOL, createPath);
        FUNCTION_DEBUG_PARAM(BOOL, syncFile);
        FUNCTION_DEBUG_PARAM(BOOL, syncPath);
        FUNCTION_DEBUG_PARAM(BOOL, atomic);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(file != NULL);
    FUNCTION_DEBUG_END();

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_DEBUG_RESULT(STORAGE_FILE_WRITE, NULL);
}

/***********************************************************************************************************************************
Create a path.  There are no physical paths on S3 so just return success.
***********************************************************************************************************************************/
void
storageDriverRemotePathCreate(StorageDriverRemote *this, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_DEBUG_PARAM(STRING, path);
        FUNCTION_DEBUG_PARAM(BOOL, errorOnExists);
        FUNCTION_DEBUG_PARAM(BOOL, noParentCreate);
        FUNCTION_DEBUG_PARAM(MODE, mode);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(path != NULL);
    FUNCTION_DEBUG_END();

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Remove a path
***********************************************************************************************************************************/
void
storageDriverRemotePathRemove(StorageDriverRemote *this, const String *path, bool errorOnMissing, bool recurse)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_DEBUG_PARAM(STRING, path);
        FUNCTION_DEBUG_PARAM(BOOL, errorOnMissing);
        FUNCTION_DEBUG_PARAM(BOOL, recurse);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(path != NULL);
    FUNCTION_DEBUG_END();

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Sync a path.  There's no need for this on S3 so just return success.
***********************************************************************************************************************************/
void
storageDriverRemotePathSync(StorageDriverRemote *this, const String *path, bool ignoreMissing)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_DEBUG_PARAM(STRING, path);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(path != NULL);
    FUNCTION_DEBUG_END();

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
void
storageDriverRemoteRemove(StorageDriverRemote *this, const String *file, bool errorOnMissing)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_REMOTE, this);
        FUNCTION_DEBUG_PARAM(STRING, file);
        FUNCTION_DEBUG_PARAM(BOOL, errorOnMissing);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(file != NULL);
    FUNCTION_DEBUG_END();

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Get storage interface
***********************************************************************************************************************************/
Storage *
storageDriverRemoteInterface(const StorageDriverRemote *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_REMOTE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STORAGE, this->interface);
}
