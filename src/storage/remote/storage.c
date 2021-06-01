/***********************************************************************************************************************************
Remote Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "common/type/object.h"
#include "common/type/pack.h"
#include "storage/remote/protocol.h"
#include "storage/remote/read.h"
#include "storage/remote/storage.intern.h"
#include "storage/remote/write.h"

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
typedef struct StorageRemoteInfoParseData
{
    PackRead *read;                                                 // Pack to read from protocol
    time_t timeModifiedLast;                                        // timeModified from last call
    mode_t modeLast;                                                // mode from last call
    uid_t userIdLast;                                               // userId from last call
    gid_t groupIdLast;                                              // groupId from last call
    String *user;                                                   // user from last call
    String *group;                                                  // group from last call
} StorageRemoteInfoParseData;

// Helper to parse storage info from the protocol output
static void
storageRemoteInfoParse(StorageRemoteInfoParseData *data, StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    // Read type and time modified
    info->type = pckReadU32P(data->read);
    info->timeModified = pckReadTimeP(data->read) + data->timeModifiedLast;

    // Read size for files
    if (info->type == storageTypeFile)
        info->size = pckReadU64P(data->read);

    // Read fields needed for detail level
    if (info->level >= storageInfoLevelDetail)
    {
        // Read mode
        info->mode = pckReadU32P(data->read, .defaultValue = data->modeLast);

        // Read user id/name
        info->userId = pckReadU32P(data->read, .defaultValue = data->userIdLast);

        if (pckReadBoolP(data->read))                                                                               // {vm_covered}
            info->user = NULL;                                                                                      // {vm_covered}
        else
            info->user = pckReadStrP(data->read, .defaultValue = data->user);

        // Read group id/name
        info->groupId = pckReadU32P(data->read, .defaultValue = data->groupIdLast);

        if (pckReadBoolP(data->read))                                                                               // {vm_covered}
            info->group = NULL;                                                                                     // {vm_covered}
        else
            info->group = pckReadStrP(data->read, .defaultValue = data->group);

        // Read link destination
        if (info->type == storageTypeLink)
            info->linkDestination = pckReadStrP(data->read);
    }

    // Store defaults to use for the next call
    data->timeModifiedLast = info->timeModified;
    data->modeLast = info->mode;
    data->userIdLast = info->userId;
    data->groupIdLast = info->groupId;

    if (!strEq(info->user, data->user) && info->user != NULL)                                                       // {vm_covered}
    {
        strFree(data->user);
        data->user = strDup(info->user);
    }

    if (!strEq(info->group, data->group) && info->group != NULL)                                                    // {vm_covered}
    {
        strFree(data->group);
        data->group = strDup(info->group);
    }

    FUNCTION_TEST_RETURN_VOID();
}

static StorageInfo
storageRemoteInfo(THIS_VOID, const String *file, StorageInfoLevel level, StorageInterfaceInfoParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(BOOL, param.followLink);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    StorageInfo result = {.level = level};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_INFO);
        PackWrite *commandParam = protocolCommandParam(command);

        pckWriteStrP(commandParam, file);
        pckWriteU32P(commandParam, level);
        pckWriteBoolP(commandParam, param.followLink);

        // Send command
        protocolClientWriteCommand(this->client, command);

        // Read info from protocol
        PackRead *read = pckReadNew(protocolClientIoRead(this->client));

        result.exists = pckReadBoolP(read);

        if (result.exists)
        {
            // Read info from protocol into prior context
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.name = strDup(result.name);

                pckReadObjBeginP(read);
                storageRemoteInfoParse(&(StorageRemoteInfoParseData){.read = read}, &result);
                pckReadObjEndP(read);
            }
            MEM_CONTEXT_PRIOR_END();
        }

        pckReadEndP(read);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
static bool
storageRemoteInfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
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
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_INFO_LIST);
        PackWrite *commandParam = protocolCommandParam(command);

        pckWriteStrP(commandParam, path);
        pckWriteU32P(commandParam, level);

        // Send command
        protocolClientWriteCommand(this->client, command);

        // Read list
        PackRead *read = pckReadNew(protocolClientIoRead(this->client));
        StorageRemoteInfoParseData parseData = {.read = read};

        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            pckReadArrayBeginP(read);

            while (!pckReadNullP(read))
            {
                pckReadObjBeginP(read);

                StorageInfo info = {.exists = true, .level = level, .name = pckReadStrP(read)};

                storageRemoteInfoParse(&parseData, &info);
                callback(callbackData, &info);

                // Reset the memory context occasionally so we don't use too much memory or slow down processing
                MEM_CONTEXT_TEMP_RESET(1000);

                pckReadObjEndP(read);
            }

            pckReadArrayEndP(read);
        }
        MEM_CONTEXT_TEMP_END();

        // Get result
        result = pckReadBoolP(read);

        pckReadEndP(read);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
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
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_PATH_CREATE);
        PackWrite *commandParam = protocolCommandParam(command);

        pckWriteStrP(commandParam, path);
        pckWriteBoolP(commandParam, errorOnExists);
        pckWriteBoolP(commandParam, noParentCreate);
        pckWriteU32P(commandParam, mode);

        protocolClientExecuteVar(this->client, command, false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_PATH_REMOVE);
        PackWrite *commandParam = protocolCommandParam(command);

        pckWriteStrP(commandParam, path);
        pckWriteBoolP(commandParam, recurse);

        result = varBool(protocolClientExecuteVar(this->client, command, true));
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
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_PATH_SYNC);
        pckWriteStrP(protocolCommandParam(command), path);

        protocolClientExecuteVar(this->client, command, false);
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
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_REMOVE);
        PackWrite *commandParam = protocolCommandParam(command);

        pckWriteStrP(commandParam, file);
        pckWriteBoolP(commandParam, param.errorOnMissing);

        protocolClientExecuteVar(this->client, command, false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageInterface storageInterfaceRemote =
{
    .info = storageRemoteInfo,
    .infoList = storageRemoteInfoList,
    .newRead = storageRemoteNewRead,
    .newWrite = storageRemoteNewWrite,
    .pathCreate = storageRemotePathCreate,
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
            protocolClientWriteCommand(driver->client, protocolCommandNew(PROTOCOL_COMMAND_STORAGE_FEATURE));

            // Get result and acknowledge command compeleted
            PackRead *result = protocolClientResult(driver->client, true);
            protocolClientResponse(driver->client);

            // Get path in parent context
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                path = pckReadStrP(result);
            }
            MEM_CONTEXT_PRIOR_END();

            driver->interface.feature = pckReadU64P(result);
        }
        MEM_CONTEXT_TEMP_END();

        this = storageNew(STORAGE_REMOTE_TYPE, path, modeFile, modePath, write, pathExpressionFunction, driver, driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}
