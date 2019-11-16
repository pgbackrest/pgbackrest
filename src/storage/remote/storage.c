/***********************************************************************************************************************************
Remote Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/type/json.h"
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
    unsigned int compressLevel;                                     // Protocol compression level
};

/***********************************************************************************************************************************
Does a file exist? This function is only for files, not paths.
***********************************************************************************************************************************/
static bool
storageRemoteExists(THIS_VOID, const String *file)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_EXISTS_STR);
        protocolCommandParamAdd(command, VARSTR(file));

        result = varBool(protocolClientExecute(this->client, command, true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
File/path info
***********************************************************************************************************************************/
// Helper to convert protocol storage type to an enum
static StorageType
storageRemoteInfoParseType(const char type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHAR, type);
    FUNCTION_TEST_END();

    switch (type)
    {
        case 'f':
            FUNCTION_TEST_RETURN(storageTypeFile);

        case 'p':
            FUNCTION_TEST_RETURN(storageTypePath);

        case 'l':
            FUNCTION_TEST_RETURN(storageTypeLink);

        case 's':
            FUNCTION_TEST_RETURN(storageTypeSpecial);
    }

    THROW_FMT(AssertError, "unknown storage type '%c'", type);
}

// Helper to parse storage info from the protocol output
static void
storageRemoteInfoParse(StorageInfo *info, IoRead *read)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
        FUNCTION_TEST_PARAM(IO_READ, read);
    FUNCTION_TEST_END();

    info->type = storageRemoteInfoParseType(strPtr(ioReadLine(read))[0]);
    info->userId = jsonToUInt(ioReadLine(read));
    info->user = jsonToStr(ioReadLine(read));
    info->groupId = jsonToUInt(ioReadLine(read));
    info->group = jsonToStr(ioReadLine(read));
    info->mode = jsonToUInt(ioReadLine(read));
    info->timeModified = (time_t)jsonToUInt64(ioReadLine(read));

    if (info->type == storageTypeFile)
        info->size = jsonToUInt64(ioReadLine(read));

    if (info->type == storageTypeLink)
        info->linkDestination = jsonToStr(ioReadLine(read));

    FUNCTION_TEST_RETURN_VOID();
}

static StorageInfo
storageRemoteInfo(THIS_VOID, const String *file, bool followLink)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, followLink);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    StorageInfo result = {.exists = false};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_INFO_STR);
        protocolCommandParamAdd(command, VARSTR(file));
        protocolCommandParamAdd(command, VARBOOL(followLink));

        result.exists = varBool(protocolClientExecute(this->client, command, true));

        if (result.exists)
        {
            // Read info from protocol
            storageRemoteInfoParse(&result, protocolClientIoRead(this->client));

            // Acknowledge command completed
            protocolClientReadOutput(this->client, false);

            // Duplicate strings into the calling context
            memContextSwitch(MEM_CONTEXT_OLD());
            result.name = strDup(result.name);
            result.linkDestination = strDup(result.linkDestination);
            result.user = strDup(result.user);
            result.group = strDup(result.group);
            memContextSwitch(MEM_CONTEXT_TEMP());
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/***********************************************************************************************************************************
Info for all files/paths in a path
***********************************************************************************************************************************/
static bool
storageRemoteInfoList(THIS_VOID, const String *path, StorageInfoListCallback callback, void *callbackData)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(FUNCTIONP, callback);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(callback != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_INFO_LIST_STR);
        protocolCommandParamAdd(command, VARSTR(path));

        // Send command
        protocolClientWriteCommand(this->client, command);

        // Read list.  The list ends when there is a blank line -- this is safe even for file systems that allow blank filenames
        // since the filename is json-encoded so will always include quotes.
        IoRead *read = protocolClientIoRead(this->client);
        const String *name = ioReadLine(read);

        while (strSize(name) != 0)
        {
            StorageInfo info = {.exists = true, .name = jsonFromStr(name)};

            storageRemoteInfoParse(&info, read);
            callback(callbackData, &info);
        }

        // Acknowledge command completed
        result = varBool(protocolClientReadOutput(this->client, true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
static StringList *
storageRemoteList(THIS_VOID, const String *path, const String *expression)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(STRING, expression);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_LIST_STR);
        protocolCommandParamAdd(command, VARSTR(path));
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
storageRemoteNewRead(THIS_VOID, const String *file, bool ignoreMissing, bool compressible)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(BOOL, compressible);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_READ,
        storageReadRemoteNew(
            this, this->client, file, ignoreMissing, this->compressLevel > 0 ? compressible : false, this->compressLevel));
}

/***********************************************************************************************************************************
New file write object
***********************************************************************************************************************************/
static StorageWrite *
storageRemoteNewWrite(
    THIS_VOID, const String *file, mode_t modeFile, mode_t modePath, const String *user, const String *group, time_t timeModified,
    bool createPath, bool syncFile, bool syncPath, bool atomic, bool compressible)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(STRING, group);
        FUNCTION_LOG_PARAM(TIME, timeModified);
        FUNCTION_LOG_PARAM(BOOL, createPath);
        FUNCTION_LOG_PARAM(BOOL, syncFile);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
        FUNCTION_LOG_PARAM(BOOL, atomic);
        FUNCTION_LOG_PARAM(BOOL, compressible);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_WRITE,
        storageWriteRemoteNew(
            this, this->client, file, modeFile, modePath, user, group, timeModified, createPath, syncFile, syncPath, atomic,
            this->compressLevel > 0 ? compressible : false, this->compressLevel));
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
        protocolCommandParamAdd(command, VARUINT(mode));

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
    ASSERT(path != NULL);

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
static bool
storageRemotePathRemove(THIS_VOID, const String *path, bool recurse)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, recurse);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_PATH_REMOVE_STR);
        protocolCommandParamAdd(command, VARSTR(path));
        protocolCommandParamAdd(command, VARBOOL(recurse));

        result = varBool(protocolClientExecute(this->client, command, true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Sync a path
***********************************************************************************************************************************/
static void
storageRemotePathSync(THIS_VOID, const String *path)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_PATH_SYNC_STR);
        protocolCommandParamAdd(command, VARSTR(path));

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
    mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction, ProtocolClient *client,
    unsigned int compressLevel)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
        FUNCTION_LOG_PARAM(UINT, compressLevel);
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
        driver->compressLevel = compressLevel;

        uint64_t feature = 0;
        const String *path = NULL;

        // Get storage features from the remote
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Send command
            protocolClientWriteCommand(driver->client, protocolCommandNew(PROTOCOL_COMMAND_STORAGE_FEATURE_STR));

            // Read values
            IoRead *read = protocolClientIoRead(driver->client);

            path = jsonToStr(ioReadLine(read));
            feature = jsonToUInt64(ioReadLine(read));

            // Acknowledge command completed
            protocolClientReadOutput(driver->client, false);

            // Dup path into parent context
            memContextSwitch(MEM_CONTEXT_OLD());
            path = strDup(path);
            memContextSwitch(MEM_CONTEXT_TEMP());
        }
        MEM_CONTEXT_TEMP_END();

        this = storageNewP(
            STORAGE_REMOTE_TYPE_STR, path, modeFile, modePath, write, pathExpressionFunction, driver, .feature = feature,
            .exists = storageRemoteExists, .info = storageRemoteInfo, .infoList = storageRemoteInfoList, .list = storageRemoteList,
            .newRead = storageRemoteNewRead, .newWrite = storageRemoteNewWrite, .pathCreate = storageRemotePathCreate,
            .pathExists = storageRemotePathExists, .pathRemove = storageRemotePathRemove, .pathSync = storageRemotePathSync,
            .remove = storageRemoteRemove);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}
