/***********************************************************************************************************************************
Remote Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
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
    ProtocolClient *client;                                         // Protocol client
    unsigned int compressLevel;                                     // Protocol compression level
};

/**********************************************************************************************************************************/
typedef struct StorageRemoteInfoData
{
    MemContext *memContext;                                         // Mem context used to store values from last call
    time_t timeModifiedLast;                                        // timeModified from last call
    mode_t modeLast;                                                // mode from last call
    uid_t userIdLast;                                               // userId from last call
    gid_t groupIdLast;                                              // groupId from last call
    String *user;                                                   // user from last call
    String *group;                                                  // group from last call
} StorageRemoteInfoData;

// Helper to get storage info from the protocol output
static void
storageRemoteInfoGet(StorageRemoteInfoData *const data, PackRead *const read, StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(PACK_READ, read);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(read != NULL);
    ASSERT(info != NULL);

    // Read type and time modified
    info->type = pckReadU32P(read);
    info->timeModified = pckReadTimeP(read) + data->timeModifiedLast;

    // Read size for files
    if (info->type == storageTypeFile)
        info->size = pckReadU64P(read);

    // Read fields needed for detail level
    if (info->level >= storageInfoLevelDetail)
    {
        // Read mode
        info->mode = pckReadModeP(read, .defaultValue = data->modeLast);

        // Read user id/name
        info->userId = pckReadU32P(read, .defaultValue = data->userIdLast);

        if (pckReadBoolP(read))                                                                                     // {vm_covered}
            info->user = NULL;                                                                                      // {vm_covered}
        else
            info->user = pckReadStrP(read, .defaultValue = data->user);

        // Read group id/name
        info->groupId = pckReadU32P(read, .defaultValue = data->groupIdLast);

        if (pckReadBoolP(read))                                                                                     // {vm_covered}
            info->group = NULL;                                                                                     // {vm_covered}
        else
            info->group = pckReadStrP(read, .defaultValue = data->group);

        // Read link destination
        if (info->type == storageTypeLink)
            info->linkDestination = pckReadStrP(read);
    }

    // Store defaults to use for the next call
    data->timeModifiedLast = info->timeModified;
    data->modeLast = info->mode;
    data->userIdLast = info->userId;
    data->groupIdLast = info->groupId;

    if (info->user != NULL && !strEq(info->user, data->user))                                                       // {vm_covered}
    {
        strFree(data->user);

        MEM_CONTEXT_BEGIN(data->memContext)
        {
            data->user = strDup(info->user);
        }
        MEM_CONTEXT_END();
    }

    if (info->group != NULL && !strEq(info->group, data->group))                                                    // {vm_covered}
    {
        strFree(data->group);

        MEM_CONTEXT_BEGIN(data->memContext)
        {
            data->group = strDup(info->group);
        }
        MEM_CONTEXT_END();
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
        PackWrite *const commandParam = protocolCommandParam(command);

        pckWriteStrP(commandParam, file);
        pckWriteU32P(commandParam, level);
        pckWriteBoolP(commandParam, param.followLink);

        // Put command
        protocolClientCommandPut(this->client, command, false);

        // Read info from protocol
        PackRead *read = protocolClientDataGet(this->client);

        result.exists = pckReadBoolP(read);

        if (result.exists)
        {
            StorageRemoteInfoData parseData = {.memContext = memContextCurrent()};

            // Read info from protocol into prior context
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                storageRemoteInfoGet(&parseData, read, &result);
            }
            MEM_CONTEXT_PRIOR_END();
        }

        protocolClientDataEndGet(this->client);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
static void
storageRemoteLinkCreate(
    THIS_VOID, const String *const target, const String *const linkPath, const StorageInterfaceLinkCreateParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, target);
        FUNCTION_LOG_PARAM(STRING, linkPath);
        FUNCTION_LOG_PARAM(ENUM, param.linkType);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(target != NULL);
    ASSERT(linkPath != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *const command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_LINK_CREATE);
        PackWrite *const commandParam = protocolCommandParam(command);

        pckWriteStrP(commandParam, target);
        pckWriteStrP(commandParam, linkPath);
        pckWriteU32P(commandParam, param.linkType);

        protocolClientExecute(this->client, command, false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static StorageList *
storageRemoteList(THIS_VOID, const String *const path, const StorageInfoLevel level, const StorageInterfaceListParam param)
{
    THIS(StorageRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    StorageList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_LIST);
        PackWrite *const commandParam = protocolCommandParam(command);

        pckWriteStrP(commandParam, path);
        pckWriteU32P(commandParam, level);

        // Put command
        protocolClientCommandPut(this->client, command, false);

        // Read list
        StorageRemoteInfoData parseData = {.memContext = memContextCurrent()};
        result = storageLstNew(level);

        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            PackRead *read = protocolClientDataGet(this->client);
            pckReadNext(read);

            while (pckReadType(read) == pckTypeStr)
            {
                StorageInfo info = {.exists = true, .level = level, .name = pckReadStrP(read)};

                storageRemoteInfoGet(&parseData, read, &info);
                storageLstAdd(result, &info);

                // Reset the memory context occasionally so we don't use too much memory or slow down processing
                MEM_CONTEXT_TEMP_RESET(1000);

                read = protocolClientDataGet(this->client);
                pckReadNext(read);
            }

            if (!pckReadBoolP(read))
                result = NULL;
        }
        MEM_CONTEXT_TEMP_END();

        protocolClientDataEndGet(this->client);
        storageLstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_LIST, result);
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
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_READ,
        storageReadRemoteNew(
            this, this->client, file, ignoreMissing, this->compressLevel > 0 ? param.compressible : false, this->compressLevel,
            param.offset, param.limit));
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
        PackWrite *const commandParam = protocolCommandParam(command);

        pckWriteStrP(commandParam, path);
        pckWriteBoolP(commandParam, errorOnExists);
        pckWriteBoolP(commandParam, noParentCreate);
        pckWriteModeP(commandParam, mode);

        protocolClientExecute(this->client, command, false);
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
        PackWrite *const commandParam = protocolCommandParam(command);

        pckWriteStrP(commandParam, path);
        pckWriteBoolP(commandParam, recurse);

        result = pckReadBoolP(protocolClientExecute(this->client, command, true));
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
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_REMOVE);
        PackWrite *const commandParam = protocolCommandParam(command);

        pckWriteStrP(commandParam, file);
        pckWriteBoolP(commandParam, param.errorOnMissing);

        protocolClientExecute(this->client, command, false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageInterface storageInterfaceRemote =
{
    .info = storageRemoteInfo,
    .list = storageRemoteList,
    .newRead = storageRemoteNewRead,
    .newWrite = storageRemoteNewWrite,
    .pathCreate = storageRemotePathCreate,
    .pathRemove = storageRemotePathRemove,
    .pathSync = storageRemotePathSync,
    .remove = storageRemoteRemove,
    .linkCreate = storageRemoteLinkCreate,
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

    OBJ_NEW_BEGIN(StorageRemote, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        StorageRemote *driver = OBJ_NEW_ALLOC();

        *driver = (StorageRemote)
        {
            .client = client,
            .compressLevel = compressLevel,
            .interface = storageInterfaceRemote,
        };

        const String *path = NULL;

        // Get storage features from the remote
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Execute command and get result
            PackRead *result = protocolClientExecute(driver->client, protocolCommandNew(PROTOCOL_COMMAND_STORAGE_FEATURE), true);

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
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}
