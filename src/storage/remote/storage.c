/***********************************************************************************************************************************
Remote Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "common/type/object.h"
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
    STORAGE_COMMON_MEMBER;
    MemContext *memContext;
    ProtocolClient *client;                                         // Protocol client
    unsigned int compressLevel;                                     // Protocol compression level
};

/**********************************************************************************************************************************/
static bool
storageRemoteExists(THIS_VOID, const String *file, StorageInterfaceExistsParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        (void)param;                                                // No parameters are used
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

/**********************************************************************************************************************************/
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
storageRemoteInfoParse(ProtocolClient *client, StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_CLIENT, client);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    info->type = storageRemoteInfoParseType(strPtr(protocolClientReadLine(client))[0]);
    info->userId = jsonToUInt(protocolClientReadLine(client));
    info->user = jsonToStr(protocolClientReadLine(client));
    info->groupId = jsonToUInt(protocolClientReadLine(client));
    info->group = jsonToStr(protocolClientReadLine(client));
    info->mode = jsonToUInt(protocolClientReadLine(client));
    info->timeModified = (time_t)jsonToUInt64(protocolClientReadLine(client));

    if (info->type == storageTypeFile)
        info->size = jsonToUInt64(protocolClientReadLine(client));

    if (info->type == storageTypeLink)
        info->linkDestination = jsonToStr(protocolClientReadLine(client));

    FUNCTION_TEST_RETURN_VOID();
}

static StorageInfo
storageRemoteInfo(THIS_VOID, const String *file, StorageInterfaceInfoParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, param.followLink);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    StorageInfo result = {.exists = false};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_INFO_STR);
        protocolCommandParamAdd(command, VARSTR(file));
        protocolCommandParamAdd(command, VARBOOL(param.followLink));

        result.exists = varBool(protocolClientExecute(this->client, command, true));

        if (result.exists)
        {
            // Read info from protocol
            storageRemoteInfoParse(this->client, &result);

            // Acknowledge command completed
            protocolClientReadOutput(this->client, false);

            // Duplicate strings into the prior context
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.name = strDup(result.name);
                result.linkDestination = strDup(result.linkDestination);
                result.user = strDup(result.user);
                result.group = strDup(result.group);
            }
            MEM_CONTEXT_PRIOR_END();
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
static bool
storageRemoteInfoList(
    THIS_VOID, const String *path, StorageInfoListCallback callback, void *callbackData, StorageInterfaceInfoListParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(FUNCTIONP, callback);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
        (void)param;                                                // No parameters are used
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
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            const String *name = protocolClientReadLine(this->client);

            while (strSize(name) != 0)
            {
                StorageInfo info = {.exists = true, .name = jsonToStr(name)};

                storageRemoteInfoParse(this->client, &info);
                callback(callbackData, &info);

                // Reset the memory context occasionally so we don't use too much memory or slow down processing
                MEM_CONTEXT_TEMP_RESET(1000);

                // Read the next item
                name = protocolClientReadLine(this->client);
            }
        }
        MEM_CONTEXT_TEMP_END();

        // Acknowledge command completed
        result = varBool(protocolClientReadOutput(this->client, true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
static StringList *
storageRemoteList(THIS_VOID, const String *path, StorageInterfaceListParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(STRING, param.expression);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_LIST_STR);
        protocolCommandParamAdd(command, VARSTR(path));
        protocolCommandParamAdd(command, VARSTR(param.expression));

        result = strLstMove(strLstNewVarLst(varVarLst(protocolClientExecute(this->client, command, true))), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
static StorageRead *
storageRemoteNewRead(THIS_VOID, const String *file, bool ignoreMissing, StorageInterfaceNewReadParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(BOOL, param.compressible);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_READ,
        storageReadRemoteNew(
            this, this->client, file, ignoreMissing, this->compressLevel > 0 ? param.compressible : false, this->compressLevel,
            param.limit));
}

/**********************************************************************************************************************************/
static StorageWrite *
storageRemoteNewWrite(
    THIS_VOID, const String *file, StorageInterfaceNewWriteParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(MODE, param.modeFile);
        FUNCTION_LOG_PARAM(MODE, param.modePath);
        FUNCTION_LOG_PARAM(STRING, param.user);
        FUNCTION_LOG_PARAM(STRING, param.group);
        FUNCTION_LOG_PARAM(TIME, param.timeModified);
        FUNCTION_LOG_PARAM(BOOL, param.createPath);
        FUNCTION_LOG_PARAM(BOOL, param.syncFile);
        FUNCTION_LOG_PARAM(BOOL, param.syncPath);
        FUNCTION_LOG_PARAM(BOOL, param.atomic);
        FUNCTION_LOG_PARAM(BOOL, param.compressible);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_WRITE,
        storageWriteRemoteNew(
            this, this->client, file, param.modeFile, param.modePath, param.user, param.group, param.timeModified, param.createPath,
            param.syncFile, param.syncPath, param.atomic, this->compressLevel > 0 ? param.compressible : false,
            this->compressLevel));
}

/**********************************************************************************************************************************/
static void
storageRemotePathCreate(
    THIS_VOID, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode, StorageInterfacePathCreateParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnExists);
        FUNCTION_LOG_PARAM(BOOL, noParentCreate);
        FUNCTION_LOG_PARAM(MODE, mode);
        (void)param;                                                // No parameters are used
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

/**********************************************************************************************************************************/
static bool
storageRemotePathExists(THIS_VOID, const String *path, StorageInterfacePathExistsParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        (void)param;                                                // No parameters are used
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

/**********************************************************************************************************************************/
static bool
storageRemotePathRemove(THIS_VOID, const String *path, bool recurse, StorageInterfacePathRemoveParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, recurse);
        (void)param;                                                // No parameters are used
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

/**********************************************************************************************************************************/
static void
storageRemotePathSync(THIS_VOID, const String *path, StorageInterfacePathSyncParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        (void)param;                                                // No parameters are used
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

/**********************************************************************************************************************************/
static void
storageRemoteRemove(THIS_VOID, const String *file, StorageInterfaceRemoveParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_REMOVE_STR);
        protocolCommandParamAdd(command, VARSTR(file));
        protocolCommandParamAdd(command, VARBOOL(param.errorOnMissing));

        protocolClientExecute(this->client, command, false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
static const StorageInterface storageInterfaceRemote =
{
    .exists = storageRemoteExists,
    .info = storageRemoteInfo,
    .infoList = storageRemoteInfoList,
    .list = storageRemoteList,
    .newRead = storageRemoteNewRead,
    .newWrite = storageRemoteNewWrite,
    .pathCreate = storageRemotePathCreate,
    .pathExists = storageRemotePathExists,
    .pathRemove = storageRemotePathRemove,
    .pathSync = storageRemotePathSync,
    .remove = storageRemoteRemove,
};

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

        *driver = (StorageRemote)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .client = client,
            .compressLevel = compressLevel,
            .interface = storageInterfaceRemote,
        };

        const String *path = NULL;

        // Get storage features from the remote
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Send command
            protocolClientWriteCommand(driver->client, protocolCommandNew(PROTOCOL_COMMAND_STORAGE_FEATURE_STR));

            // Read values
            path = jsonToStr(protocolClientReadLine(driver->client));
            driver->interface.feature = jsonToUInt64(protocolClientReadLine(driver->client));

            // Acknowledge command completed
            protocolClientReadOutput(driver->client, false);

            // Dup path into parent context
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                path = strDup(path);
            }
            MEM_CONTEXT_PRIOR_END();
        }
        MEM_CONTEXT_TEMP_END();

        this = storageNew(
            STORAGE_REMOTE_TYPE_STR, path, modeFile, modePath, write, pathExpressionFunction, driver, driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}
