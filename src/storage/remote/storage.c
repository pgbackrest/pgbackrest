/***********************************************************************************************************************************
Remote Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "storage/remote/protocol.h"
#include "storage/remote/read.h"
#include "storage/remote/storage.intern.h"
#include "storage/remote/write.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_REMOTE_TYPE_STR,                              STORAGE_REMOTE_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageRemote
{
    MemContext *memContext;
    ProtocolClient *client;                                         // Protocol client
};

/***********************************************************************************************************************************
Does a file exist? This function is only for files, not paths.
***********************************************************************************************************************************/
static bool
storageRemoteExists(THIS_VOID, const String *path)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_EXISTS_STR);
        protocolCommandParamAdd(command, VARSTR(path));

        result = varBool(protocolClientExecute(this->client, command, true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
File/path info
***********************************************************************************************************************************/
static StorageInfo
storageRemoteInfo(THIS_VOID, const String *file, bool ignoreMissing, bool followLink)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(BOOL, followLink);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN(STORAGE_INFO, (StorageInfo){0});
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
static StringList *
storageRemoteList(THIS_VOID, const String *path, bool errorOnMissing, const String *expression)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
        FUNCTION_LOG_PARAM(STRING, expression);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!errorOnMissing);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_LIST_STR);
        protocolCommandParamAdd(command, VARSTR(path));
        protocolCommandParamAdd(command, VARBOOL(errorOnMissing));
        protocolCommandParamAdd(command, VARSTR(expression));

        result = strLstMove(strLstNewVarLst(varVarLst(protocolClientExecute(this->client, command, true))), MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
New file read object
***********************************************************************************************************************************/
static StorageRead *
storageRemoteNewRead(THIS_VOID, const String *file, bool ignoreMissing)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(STORAGE_READ, storageReadRemoteNew(this, this->client, file, ignoreMissing));
}

/***********************************************************************************************************************************
New file write object
***********************************************************************************************************************************/
static StorageWrite *
storageRemoteNewWrite(
    THIS_VOID, const String *file, mode_t modeFile, mode_t modePath, const String *user, const String *group, time_t timeModified,
    bool createPath, bool syncFile, bool syncPath, bool atomic)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(STRING, group);
        FUNCTION_LOG_PARAM(INT64, timeModified);
        FUNCTION_LOG_PARAM(BOOL, createPath);
        FUNCTION_LOG_PARAM(BOOL, syncFile);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
        FUNCTION_LOG_PARAM(BOOL, atomic);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_WRITE,
        storageWriteRemoteNew(
            this, this->client, file, modeFile, modePath, user, group, timeModified, createPath, syncFile, syncPath, atomic));
}

/***********************************************************************************************************************************
Create a path.  There are no physical paths on S3 so just return success.
***********************************************************************************************************************************/
static void
storageRemotePathCreate(THIS_VOID, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnExists);
        FUNCTION_LOG_PARAM(BOOL, noParentCreate);
        FUNCTION_LOG_PARAM(MODE, mode);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_PATH_CREATE_STR);
        protocolCommandParamAdd(command, VARSTR(path));
        protocolCommandParamAdd(command, VARBOOL(errorOnExists));
        protocolCommandParamAdd(command, VARBOOL(noParentCreate));
        protocolCommandParamAdd(command, VARUINT64(mode));

        protocolClientExecute(this->client, command, false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Does a path exist?
***********************************************************************************************************************************/
static bool
storageRemotePathExists(THIS_VOID, const String *path)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_PATH_EXISTS_STR);
        protocolCommandParamAdd(command, VARSTR(path));

        result = varBool(protocolClientExecute(this->client, command, true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Remove a path
***********************************************************************************************************************************/
static void
storageRemotePathRemove(THIS_VOID, const String *path, bool errorOnMissing, bool recurse)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
        FUNCTION_LOG_PARAM(BOOL, recurse);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_PATH_REMOVE_STR);
        protocolCommandParamAdd(command, VARSTR(path));
        protocolCommandParamAdd(command, VARBOOL(errorOnMissing));
        protocolCommandParamAdd(command, VARBOOL(recurse));

        protocolClientExecute(this->client, command, false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Sync a path.  There's no need for this on S3 so just return success.
***********************************************************************************************************************************/
static void
storageRemotePathSync(THIS_VOID, const String *path, bool ignoreMissing)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_PATH_SYNC_STR);
        protocolCommandParamAdd(command, VARSTR(path));
        protocolCommandParamAdd(command, VARBOOL(ignoreMissing));

        protocolClientExecute(this->client, command, false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
static void
storageRemoteRemove(THIS_VOID, const String *file, bool errorOnMissing)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_REMOVE_STR);
        protocolCommandParamAdd(command, VARSTR(file));
        protocolCommandParamAdd(command, VARBOOL(errorOnMissing));

        protocolClientExecute(this->client, command, false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
Storage *
storageRemoteNew(
    mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction, ProtocolClient *client)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
    FUNCTION_LOG_END();

    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);
    ASSERT(client != NULL);

    Storage *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageRemote")
    {
        StorageRemote *driver = memNew(sizeof(StorageRemote));
        driver->memContext = MEM_CONTEXT_NEW();
        driver->client = client;

        this = storageNewP(
            STORAGE_REMOTE_TYPE_STR, NULL, modeFile, modePath, write, pathExpressionFunction, driver,
            .exists = storageRemoteExists, .info = storageRemoteInfo, .list = storageRemoteList, .newRead = storageRemoteNewRead,
            .newWrite = storageRemoteNewWrite, .pathCreate = storageRemotePathCreate, .pathExists = storageRemotePathExists,
            .pathRemove = storageRemotePathRemove, .pathSync = storageRemotePathSync, .remove = storageRemoteRemove);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}
