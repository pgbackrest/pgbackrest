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
#include "common/regExp.h"
#include "common/type/pack.h"
#include "config/config.h"
#include "protocol/helper.h"
#include "storage/remote/protocol.h"
#include "storage/helper.h"
#include "storage/storage.intern.h"

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
storageRemoteFilterGroup(IoFilterGroup *const filterGroup, const Pack *const filterPack)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, filterGroup);
        FUNCTION_TEST_PARAM(PACK, filterPack);
    FUNCTION_TEST_END();

    ASSERT(filterGroup != NULL);
    ASSERT(filterPack != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const filterList = pckReadNew(filterPack);

        while (!pckReadNullP(filterList))
        {
            const StringId filterKey = pckReadStrIdP(filterList);
            const Pack *const filterParam = pckReadPackP(filterList);

            IoFilter *filter = compressFilterPack(filterKey, filterParam);

            if (filter != NULL)
                ioFilterGroupAdd(filterGroup, filter);
            else
            {
                switch (filterKey)
                {
                    case CIPHER_BLOCK_FILTER_TYPE:
                        ioFilterGroupAdd(filterGroup, cipherBlockNewPack(filterParam));
                        break;

                    case CRYPTO_HASH_FILTER_TYPE:
                        ioFilterGroupAdd(filterGroup, cryptoHashNewPack(filterParam));
                        break;

                    case PAGE_CHECKSUM_FILTER_TYPE:
                        ioFilterGroupAdd(filterGroup, pageChecksumNewPack(filterParam));
                        break;

                    case SINK_FILTER_TYPE:
                        ioFilterGroupAdd(filterGroup, ioSinkNew());
                        break;

                    case SIZE_FILTER_TYPE:
                        ioFilterGroupAdd(filterGroup, ioSizeNew());
                        break;

                    default:
                        THROW_FMT(AssertError, "unable to add filter '%s'", strZ(strIdToStr(filterKey)));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

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
                MEM_CONTEXT_NEW_BEGIN(StorageRemoteProtocol, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
                {
                    storageRemoteProtocolLocal.memContext = memContextCurrent();
                    storageRemoteProtocolLocal.driver = storageDriver(storage);
                }
                MEM_CONTEXT_NEW_END();
            }
            MEM_CONTEXT_PRIOR_END();
        }

        // Return storage features
        PackWrite *result = protocolPackNew();
        pckWriteStrP(result, storagePathP(storage, NULL));
        pckWriteU64P(result, storageInterface(storage).feature);

        protocolServerDataPut(server, result);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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
storageRemoteInfoProtocolPut(
    StorageRemoteInfoProtocolWriteData *const data, PackWrite *const write, const StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(PACK_WRITE, write);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(write != NULL);
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
        pckWriteModeP(write, info->mode, .defaultValue = data->modeLast);

        // Write user id/name
        pckWriteU32P(write, info->userId, .defaultValue = data->userIdLast);

        if (info->user == NULL)                                                                                     // {vm_covered}
            pckWriteBoolP(write, true);                                                                             // {vm_covered}
        else
        {
            pckWriteBoolP(write, false);
            pckWriteStrP(write, info->user, .defaultValue = data->user);
        }

        // Write group id/name
        pckWriteU32P(write, info->groupId, .defaultValue = data->groupIdLast);

        if (info->group == NULL)                                                                                    // {vm_covered}
            pckWriteBoolP(write, true);                                                                             // {vm_covered}
        else
        {
            pckWriteBoolP(write, false);
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

        if (info->user != NULL && !strEq(info->user, data->user))                                                   // {vm_covered}
        {
            strFree(data->user);

            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->user = strDup(info->user);
            }
            MEM_CONTEXT_END();
        }

        if (info->group != NULL && !strEq(info->group, data->group))                                                // {vm_covered}
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
        PackWrite *write = protocolPackNew();
        pckWriteBoolP(write, info.exists, .defaultWrite = true);

        if (info.exists)
            storageRemoteInfoProtocolPut(&(StorageRemoteInfoProtocolWriteData){0}, write, &info);

        protocolServerDataPut(server, write);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteLinkCreateProtocol(PackRead *const param, ProtocolServer *const server)
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
        const String *const target = pckReadStrP(param);
        const String *const linkPath = pckReadStrP(param);
        const StorageLinkType linkType = (StorageLinkType)pckReadU32P(param);

        storageInterfaceLinkCreateP(storageRemoteProtocolLocal.driver, target, linkPath, .linkType = linkType);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteListProtocol(PackRead *const param, ProtocolServer *const server)
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
        StorageRemoteInfoProtocolWriteData writeData = {.memContext = memContextCurrent()};
        StorageList *const list = storageInterfaceListP(storageRemoteProtocolLocal.driver, path, level);

        // Put list
        if (list != NULL)
        {
            for (unsigned int listIdx = 0; listIdx < storageLstSize(list); listIdx++)
            {
                const StorageInfo info = storageLstGet(list, listIdx);

                PackWrite *const write = protocolPackNew();
                pckWriteStrP(write, info.name);
                storageRemoteInfoProtocolPut(&writeData, write, &info);
                protocolServerDataPut(server, write);
                pckWriteFree(write);
            }
        }

        // Indicate whether or not the path was found
        PackWrite *write = protocolPackNew();
        pckWriteBoolP(write, list != NULL, .defaultWrite = true);
        protocolServerDataPut(server, write);

        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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
        const uint64_t offset = pckReadU64P(param);
        const Variant *const limit = pckReadNullP(param) ? NULL : VARUINT64(pckReadU64P(param));
        const Pack *const filter = pckReadPackP(param);

        // Create the read object
        IoRead *fileRead = storageReadIo(
            storageInterfaceNewReadP(storageRemoteProtocolLocal.driver, file, ignoreMissing, .offset = offset, .limit = limit));

        // Set filter group based on passed filters
        storageRemoteFilterGroup(ioReadFilterGroup(fileRead), filter);

        // Check if the file exists
        bool exists = ioReadOpen(fileRead);
        protocolServerDataPut(server, pckWriteBoolP(protocolPackNew(), exists, .defaultWrite = true));

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
                    MEM_CONTEXT_TEMP_BEGIN()
                    {
                        PackWrite *write = pckWriteNewP(.size = ioBufferSize() + PROTOCOL_PACK_DEFAULT_SIZE);
                        pckWriteBinP(write, buffer);
                        protocolServerDataPut(server, write);
                    }
                    MEM_CONTEXT_TEMP_END();

                    bufUsedZero(buffer);
                }
            }
            while (!ioReadEof(fileRead));

            ioReadClose(fileRead);

            // Write filter results
            protocolServerDataPut(server, pckWritePackP(protocolPackNew(), ioFilterGroupResultAll(ioReadFilterGroup(fileRead))));
        }

        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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
        mode_t modeFile = pckReadModeP(param);
        mode_t modePath = pckReadModeP(param);
        const String *user = pckReadStrP(param);
        const String *group = pckReadStrP(param);
        time_t timeModified = pckReadTimeP(param);
        bool createPath = pckReadBoolP(param);
        bool syncFile = pckReadBoolP(param);
        bool syncPath = pckReadBoolP(param);
        bool atomic = pckReadBoolP(param);
        const Pack *const filter = pckReadPackP(param);

        IoWrite *fileWrite = storageWriteIo(
            storageInterfaceNewWriteP(
                storageRemoteProtocolLocal.driver, file, .modeFile = modeFile, .modePath = modePath, .user = user, .group = group,
                .timeModified = timeModified, .createPath = createPath, .syncFile = syncFile, .syncPath = syncPath,
                .atomic = atomic));

        // Set filter group based on passed filters
        storageRemoteFilterGroup(ioWriteFilterGroup(fileWrite), filter);

        // Open file
        ioWriteOpen(fileWrite);
        protocolServerDataPut(server, NULL);

        // Write data
        do
        {
            PackRead *read = protocolServerDataGet(server);

            // Write is complete
            if (read == NULL)
            {
                ioWriteClose(fileWrite);

                // Push filter results
                protocolServerDataPut(
                    server, pckWritePackP(protocolPackNew(), ioFilterGroupResultAll(ioWriteFilterGroup(fileWrite))));
                break;
            }
            // Else more data to write
            else
            {
                pckReadNext(read);

                // Write data
                if (pckReadType(read) == pckTypeBin)
                {
                    Buffer *const buffer = pckReadBinP(read);

                    ioWrite(fileWrite, buffer);
                    bufFree(buffer);
                }
                // Else write terminated unexpectedly
                else
                {
                    protocolServerDataGet(server);
                    ioWriteFree(fileWrite);
                    break;
                }
            }
        }
        while (true);

        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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
        mode_t mode = pckReadModeP(param);

        storageInterfacePathCreateP(storageRemoteProtocolLocal.driver, path, errorOnExists, noParentCreate, mode);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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

        const bool result = storageInterfacePathRemoveP(storageRemoteProtocolLocal.driver, path, recurse);

        protocolServerDataPut(server, pckWriteBoolP(protocolPackNew(), result, .defaultWrite = true));
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
