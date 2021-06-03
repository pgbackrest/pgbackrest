/***********************************************************************************************************************************
Remote Storage Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/pageChecksum.h"
#include "common/compress/helper.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/filter/sink.h"
#include "common/io/filter/size.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/type/pack.h"
#include "common/type/json.h"
#include "config/config.h"
#include "protocol/helper.h"
#include "storage/remote/protocol.h"
#include "storage/helper.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Regular expressions
***********************************************************************************************************************************/
STRING_STATIC(BLOCK_REG_EXP_STR,                                    PROTOCOL_BLOCK_HEADER "-1|[0-9]+"); // REMOVE

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct
{
    MemContext *memContext;                                         // Mem context
    void *driver;                                                   // Storage driver used for requests
} storageRemoteProtocolLocal;

/***********************************************************************************************************************************
Set filter group based on passed filters
***********************************************************************************************************************************/
static void
storageRemoteFilterGroup(IoFilterGroup *filterGroup, const Variant *filterList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, filterGroup);
        FUNCTION_TEST_PARAM(VARIANT, filterList);
    FUNCTION_TEST_END();

    ASSERT(filterGroup != NULL);
    ASSERT(filterList != NULL);

    for (unsigned int filterIdx = 0; filterIdx < varLstSize(varVarLst(filterList)); filterIdx++)
    {
        const KeyValue *filterKv = varKv(varLstGet(varVarLst(filterList), filterIdx));
        const String *filterKey = varStr(varLstGet(kvKeyList(filterKv), 0));
        const VariantList *filterParam = varVarLst(kvGet(filterKv, VARSTR(filterKey)));

        IoFilter *filter = compressFilterVar(filterKey, filterParam);

        if (filter != NULL)
            ioFilterGroupAdd(filterGroup, filter);
        else if (strEq(filterKey, CIPHER_BLOCK_FILTER_TYPE_STR))
            ioFilterGroupAdd(filterGroup, cipherBlockNewVar(filterParam));
        else if (strEq(filterKey, CRYPTO_HASH_FILTER_TYPE_STR))
            ioFilterGroupAdd(filterGroup, cryptoHashNewVar(filterParam));
        else if (strEq(filterKey, PAGE_CHECKSUM_FILTER_TYPE_STR))
            ioFilterGroupAdd(filterGroup, pageChecksumNewVar(filterParam));
        else if (strEq(filterKey, SINK_FILTER_TYPE_STR))
            ioFilterGroupAdd(filterGroup, ioSinkNew());
        else if (strEq(filterKey, SIZE_FILTER_TYPE_STR))
            ioFilterGroupAdd(filterGroup, ioSizeNew());
        else
            THROW_FMT(AssertError, "unable to add filter '%s'", strZ(filterKey));
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteFeatureProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get storage based on remote type
        const Storage *storage =
            cfgOptionStrId(cfgOptRemoteType) == protocolStorageTypeRepo ? storageRepoWrite() : storagePgWrite();

        // Store local variables in the server context
        if (storageRemoteProtocolLocal.memContext == NULL)
        {
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                MEM_CONTEXT_NEW_BEGIN("StorageRemoteProtocol")
                {
                    storageRemoteProtocolLocal.memContext = memContextCurrent();
                    storageRemoteProtocolLocal.driver = storageDriver(storage);
                }
                MEM_CONTEXT_NEW_END();
            }
            MEM_CONTEXT_PRIOR_END();
        }

        // Return storage features
        PackWrite *result = protocolServerResultPack();
        pckWriteStrP(result, storagePathP(storage, NULL));
        pckWriteU64P(result, storageInterface(storage).feature);

        protocolServerResult(server, result);
        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
typedef struct StorageRemoteInfoProcotolWriteData
{
    MemContext *memContext;                                         // Mem context used to store values from last call
    time_t timeModifiedLast;                                        // timeModified from last call
    mode_t modeLast;                                                // mode from last call
    uid_t userIdLast;                                               // userId from last call
    gid_t groupIdLast;                                              // groupId from last call
    String *user;                                                   // user from last call
    String *group;                                                  // group from last call
} StorageRemoteInfoProtocolWriteData;

// Helper to write storage info into the protocol. This function is not called unless the info exists so no need to write exists or
// check for level == storageInfoLevelExists.
//
// Fields that do not change from one call to the next are omitted to save bandwidth.
static void
storageRemoteInfoProtocolWrite(
    StorageRemoteInfoProtocolWriteData *const data, PackWrite *const write, const StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(info != NULL);

    // Write type and time
    pckWriteU32P(write, info->type);
    pckWriteTimeP(write, info->timeModified - data->timeModifiedLast);

    // Write size for files
    if (info->type == storageTypeFile)
        pckWriteU64P(write, info->size);

    // Write fields needed for detail level
    if (info->level >= storageInfoLevelDetail)
    {
        // Write mode
        pckWriteU32P(write, info->mode, .defaultValue = data->modeLast);

        // Write user id/name
        pckWriteU32P(write, info->userId, .defaultValue = data->userIdLast);

        if (info->user == NULL)                                                                                     // {vm_covered}
            pckWriteBoolP(write, true);                                                                             // {vm_covered}
        else
        {
            pckWriteNullP(write);
            pckWriteStrP(write, info->user, .defaultValue = data->user);
        }

        // Write group id/name
        pckWriteU32P(write, info->groupId, .defaultValue = data->groupIdLast);

        if (info->group == NULL)                                                                                    // {vm_covered}
            pckWriteBoolP(write, true);                                                                             // {vm_covered}
        else
        {
            pckWriteNullP(write);
            pckWriteStrP(write, info->group, .defaultValue = data->group);
        }

        // Write link destination
        if (info->type == storageTypeLink)
            pckWriteStrP(write, info->linkDestination);
    }

    // Store defaults to use for the next call. If memContext is NULL this function is only being called one time so there is no
    // point in storing defaults.
    if (data->memContext != NULL)
    {
        data->timeModifiedLast = info->timeModified;
        data->modeLast = info->mode;
        data->userIdLast = info->userId;
        data->groupIdLast = info->groupId;

        if (!strEq(info->user, data->user) && info->user != NULL)                                                   // {vm_covered}
        {
            strFree(data->user);

            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->user = strDup(info->user);
            }
            MEM_CONTEXT_END();
        }

        if (!strEq(info->group, data->group) && info->group != NULL)                                                // {vm_covered}
        {
            strFree(data->group);

            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->group = strDup(info->group);
            }
            MEM_CONTEXT_END();
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

void
storageRemoteInfoProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get file info
        const String *file = pckReadStrP(param);
        StorageInfoLevel level = (StorageInfoLevel)pckReadU32P(param);
        bool followLink = pckReadBoolP(param);

        StorageInfo info = storageInterfaceInfoP(storageRemoteProtocolLocal.driver, file, level, .followLink = followLink);

        // Write file info to protocol
        PackWrite *write = protocolServerResultPack();
        pckWriteBoolP(write, info.exists, .defaultWrite = true);

        if (info.exists)
            storageRemoteInfoProtocolWrite(&(StorageRemoteInfoProtocolWriteData){}, write, &info);

        protocolServerResult(server, write);
        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
typedef struct StorageRemoteProtocolInfoListCallbackData
{
    ProtocolServer *const server;
    StorageRemoteInfoProtocolWriteData writeData;
} StorageRemoteProtocolInfoListCallbackData;

static void
storageRemoteProtocolInfoListCallback(void *dataVoid, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM_P(VOID, dataVoid);
        FUNCTION_LOG_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(dataVoid != NULL);
    ASSERT(info != NULL);

    StorageRemoteProtocolInfoListCallbackData *data = dataVoid;

    PackWrite *write = protocolServerResultPack();
    pckWriteStrP(write, info->name);
    storageRemoteInfoProtocolWrite(&data->writeData, write, info);
    protocolServerResult(data->server, write);

    FUNCTION_TEST_RETURN_VOID();
}

void
storageRemoteInfoListProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const path = pckReadStrP(param);
        const StorageInfoLevel level = (StorageInfoLevel)pckReadU32P(param);

        StorageRemoteProtocolInfoListCallbackData data = {.server = server, .writeData = {.memContext = memContextCurrent()}};

        const bool result = storageInterfaceInfoListP(
            storageRemoteProtocolLocal.driver, path, level, storageRemoteProtocolInfoListCallback, &data);

        // Indicate whether or not the path was found
        PackWrite *write = protocolServerResultPack();
        pckWriteBoolP(write, result, .defaultWrite = true);
        protocolServerResult(server, write);

        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteOpenReadProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *file = pckReadStrP(param);
        bool ignoreMissing = pckReadBoolP(param);
        const Variant *limit = jsonToVar(pckReadStrP(param));
        const Variant *filter = jsonToVar(pckReadStrP(param));

        // Create the read object
        IoRead *fileRead = storageReadIo(
            storageInterfaceNewReadP(storageRemoteProtocolLocal.driver, file, ignoreMissing, .limit = limit));

        // Set filter group based on passed filters
        storageRemoteFilterGroup(ioReadFilterGroup(fileRead), filter);

        // Check if the file exists
        bool exists = ioReadOpen(fileRead);
        protocolServerResultBool(server, exists);

        // Transfer the file if it exists
        if (exists)
        {
            Buffer *buffer = bufNew(ioBufferSize());

            // Write file out to protocol layer
            do
            {
                ioRead(fileRead, buffer);

                if (!bufEmpty(buffer))
                {
                    PackWrite *write = protocolServerResultPack();
                    pckWriteBinP(write, buffer);
                    protocolServerResult(server, write);

                    bufUsedZero(buffer);
                }
            }
            while (!ioReadEof(fileRead));

            ioReadClose(fileRead);

            // Write filter results
            protocolServerResultVar(server, ioFilterGroupResultAll(ioReadFilterGroup(fileRead)));
        }

        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteOpenWriteProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create the write object
        const String *file = pckReadStrP(param);
        mode_t modeFile = pckReadU32P(param);
        mode_t modePath = pckReadU32P(param);
        const String *user = pckReadStrP(param);
        const String *group = pckReadStrP(param);
        time_t timeModified = pckReadTimeP(param);
        bool createPath = pckReadBoolP(param);
        bool syncFile = pckReadBoolP(param);
        bool syncPath = pckReadBoolP(param);
        bool atomic = pckReadBoolP(param);
        const Variant *filter = jsonToVar(pckReadStrP(param));

        IoWrite *fileWrite = storageWriteIo(
            storageInterfaceNewWriteP(
                storageRemoteProtocolLocal.driver, file, .modeFile = modeFile, .modePath = modePath, .user = user, .group = group,
                .timeModified = timeModified, .createPath = createPath, .syncFile = syncFile, .syncPath = syncPath,
                .atomic = atomic));

        // Set filter group based on passed filters
        storageRemoteFilterGroup(ioWriteFilterGroup(fileWrite), filter);

        // Open file
        ioWriteOpen(fileWrite);
        protocolServerResponseVar(server, NULL);

        // Write data
        Buffer *buffer = bufNew(ioBufferSize());
        ssize_t remaining;

        do
        {
            // How much data is remaining to write?
            remaining = storageRemoteProtocolBlockSize(ioReadLine(protocolServerIoRead(server)));

            // Write data
            if (remaining > 0)
            {
                size_t bytesToCopy = (size_t)remaining;

                do
                {
                    if (bytesToCopy < bufSize(buffer))
                        bufLimitSet(buffer, bytesToCopy);

                    bytesToCopy -= ioRead(protocolServerIoRead(server), buffer);
                    ioWrite(fileWrite, buffer);

                    bufUsedZero(buffer);
                    bufLimitClear(buffer);
                }
                while (bytesToCopy > 0);
            }
            // Close when all data has been written
            else if (remaining == 0)
            {
                ioWriteClose(fileWrite);

                // Push filter results
                protocolServerResponseVar(server, ioFilterGroupResultAll(ioWriteFilterGroup(fileWrite)));
            }
            // Write was aborted so free the file
            else
            {
                ioWriteFree(fileWrite);
                protocolServerResponseVar(server, NULL);
            }
        }
        while (remaining > 0);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemotePathCreateProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *path = pckReadStrP(param);
        bool errorOnExists = pckReadBoolP(param);
        bool noParentCreate = pckReadBoolP(param);
        mode_t mode = pckReadU32P(param);

        storageInterfacePathCreateP(storageRemoteProtocolLocal.driver, path, errorOnExists, noParentCreate, mode);
        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemotePathRemoveProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *path = pckReadStrP(param);
        bool recurse = pckReadBoolP(param);

        protocolServerResultBool(server, VARBOOL(storageInterfacePathRemoveP(storageRemoteProtocolLocal.driver, path, recurse)));
        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemotePathSyncProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *path = pckReadStrP(param);

        storageInterfacePathSyncP(storageRemoteProtocolLocal.driver, path);
        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteRemoveProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *file = pckReadStrP(param);
        bool errorOnMissing = pckReadBoolP(param);

        storageInterfaceRemoveP(storageRemoteProtocolLocal.driver, file, .errorOnMissing = errorOnMissing);
        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
// !!! REMOVE
ssize_t
storageRemoteProtocolBlockSize(const String *message)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, message);
    FUNCTION_LOG_END();

    ASSERT(message != NULL);

    // Regular expression to check block messages
    static RegExp *blockRegExp = NULL;

    // Create block regular expression if it has not been created yet
    if (blockRegExp == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            blockRegExp = regExpNew(BLOCK_REG_EXP_STR);
        }
        MEM_CONTEXT_END();
    }

    // Validate the header block size message
    if (!regExpMatch(blockRegExp, message))
        THROW_FMT(ProtocolError, "'%s' is not a valid block size message", strZ(message));

    FUNCTION_LOG_RETURN(SSIZE, (ssize_t)cvtZToInt(strZ(message) + sizeof(PROTOCOL_BLOCK_HEADER) - 1));
}
