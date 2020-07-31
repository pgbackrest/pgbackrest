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

// Helper to parse storage info from the protocol output
static void
storageRemoteInfoParse(PackRead *read, StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, read);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    info->type = pckReadUInt32P(read);
    info->timeModified = (time_t)pckReadInt64P(read);

    if (info->type == storageTypeFile)
        info->size = pckReadUInt64P(read);

    if (info->level >= storageInfoLevelDetail)
    {
        info->userId = pckReadUInt32P(read);
        info->user = pckReadStrNullP(read);
        info->groupId = pckReadUInt32P(read);
        info->group = pckReadStrNullP(read);
        info->mode = pckReadUInt32P(read);

        if (info->type == storageTypeLink)
            info->linkDestination = pckReadStrP(read);
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
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_INFO_STR);
        protocolCommandParamAdd(command, VARSTR(file));
        protocolCommandParamAdd(command, VARUINT(level));
        protocolCommandParamAdd(command, VARBOOL(param.followLink));

        // Send command
        protocolClientWriteCommand(this->client, command);

        // Read info from protocol
        PackRead *read = pckReadNew(protocolClientIoRead(this->client));

        result.exists = pckReadBoolP(read);

        if (result.exists)
        {
            storageRemoteInfoParse(read, &result);

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

        pckReadEnd(read);
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
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_INFO_LIST_STR);
        protocolCommandParamAdd(command, VARSTR(path));
        protocolCommandParamAdd(command, VARUINT(level));

        // Send command
        protocolClientWriteCommand(this->client, command);

        // Read list.  The list ends when there is a blank line -- this is safe even for file systems that allow blank filenames
        // since the filename is json-encoded so will always include quotes.
        PackRead *read = pckReadNew(protocolClientIoRead(this->client));

        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            pckReadArrayBeginP(read);

            while (pckReadNext(read))
            {
                pckReadObjBeginP(read);

                StorageInfo info = {.exists = true, .level = level, .name = pckReadStrP(read)};

                storageRemoteInfoParse(read, &info);
                callback(callbackData, &info);

                // Reset the memory context occasionally so we don't use too much memory or slow down processing
                MEM_CONTEXT_TEMP_RESET(1000);

                pckReadObjEnd(read);
            }

            pckReadArrayEnd(read);
        }
        MEM_CONTEXT_TEMP_END();

        // Get result
        result = pckReadBoolP(read);

        pckReadEnd(read);
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
