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
#include "common/type/json.h"
#include "config/config.h"
#include "protocol/helper.h"
#include "storage/remote/protocol.h"
#include "storage/helper.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Regular expressions
***********************************************************************************************************************************/
STRING_STATIC(BLOCK_REG_EXP_STR,                                    PROTOCOL_BLOCK_HEADER "-1|[0-9]+");

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

/***********************************************************************************************************************************
Callback to write info list into the protocol
***********************************************************************************************************************************/
// Helper to write storage info into the protocol. This function is not called unless the info exists so no need to write exists or
// check for level == storageInfoLevelExists.
static void
storageRemoteInfoWrite(ProtocolServer *server, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_SERVER, server);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    protocolServerWriteLine(server, jsonFromUInt(info->type));
    protocolServerWriteLine(server, jsonFromInt64(info->timeModified));

    if (info->type == storageTypeFile)
        protocolServerWriteLine(server, jsonFromUInt64(info->size));

    if (info->level >= storageInfoLevelDetail)
    {
        protocolServerWriteLine(server, jsonFromUInt(info->userId));
        protocolServerWriteLine(server, jsonFromStr(info->user));
        protocolServerWriteLine(server, jsonFromUInt(info->groupId));
        protocolServerWriteLine(server, jsonFromStr(info->group));
        protocolServerWriteLine(server, jsonFromUInt(info->mode));

        if (info->type == storageTypeLink)
            protocolServerWriteLine(server, jsonFromStr(info->linkDestination));
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
storageRemoteProtocolInfoListCallback(void *server, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
        FUNCTION_LOG_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    protocolServerWriteLine(server, jsonFromStr(info->name));
    storageRemoteInfoWrite(server, info);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteFeatureProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList == NULL);
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
        protocolServerWriteLine(server, jsonFromStr(storagePathP(storage, NULL)));
        protocolServerWriteLine(server, jsonFromUInt64(storageInterface(storage).feature));

        protocolServerResponse(server, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteInfoProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StorageInfo info = storageInterfaceInfoP(
            storageRemoteProtocolLocal.driver, varStr(varLstGet(paramList, 0)),
            (StorageInfoLevel)varUIntForce(varLstGet(paramList, 1)), .followLink = varBool(varLstGet(paramList, 2)));

        protocolServerResponse(server, VARBOOL(info.exists));

        if (info.exists)
        {
            storageRemoteInfoWrite(server, &info);
            protocolServerResponse(server, NULL);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteInfoListProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        bool result = storageInterfaceInfoListP(
            storageRemoteProtocolLocal.driver, varStr(varLstGet(paramList, 0)),
            (StorageInfoLevel)varUIntForce(varLstGet(paramList, 1)), storageRemoteProtocolInfoListCallback, server);

        protocolServerWriteLine(server, NULL);
        protocolServerResponse(server, VARBOOL(result));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteOpenReadProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create the read object
        IoRead *fileRead = storageReadIo(
            storageInterfaceNewReadP(
                storageRemoteProtocolLocal.driver, varStr(varLstGet(paramList, 0)), varBool(varLstGet(paramList, 1)),
                .limit = varLstGet(paramList, 2)));

        // Set filter group based on passed filters
        storageRemoteFilterGroup(ioReadFilterGroup(fileRead), varLstGet(paramList, 3));

        // Check if the file exists
        bool exists = ioReadOpen(fileRead);
        protocolServerResponse(server, VARBOOL(exists));

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
                    ioWriteStrLine(protocolServerIoWrite(server), strNewFmt(PROTOCOL_BLOCK_HEADER "%zu", bufUsed(buffer)));
                    ioWrite(protocolServerIoWrite(server), buffer);
                    ioWriteFlush(protocolServerIoWrite(server));

                    bufUsedZero(buffer);
                }
            }
            while (!ioReadEof(fileRead));

            ioReadClose(fileRead);

            // Write a zero block to show file is complete
            ioWriteLine(protocolServerIoWrite(server), BUFSTRDEF(PROTOCOL_BLOCK_HEADER "0"));
            ioWriteFlush(protocolServerIoWrite(server));

            // Push filter results
            protocolServerResponse(server, ioFilterGroupResultAll(ioReadFilterGroup(fileRead)));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteOpenWriteProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create the write object
        IoWrite *fileWrite = storageWriteIo(
            storageInterfaceNewWriteP(
                storageRemoteProtocolLocal.driver, varStr(varLstGet(paramList, 0)),
                .modeFile = (mode_t)varUIntForce(varLstGet(paramList, 1)),
                .modePath = (mode_t)varUIntForce(varLstGet(paramList, 2)), .user = varStr(varLstGet(paramList, 3)),
                .group = varStr(varLstGet(paramList, 4)), .timeModified = (time_t)varUInt64Force(varLstGet(paramList, 5)),
                .createPath = varBool(varLstGet(paramList, 6)), .syncFile = varBool(varLstGet(paramList, 7)),
                .syncPath = varBool(varLstGet(paramList, 8)), .atomic = varBool(varLstGet(paramList, 9))));

        // Set filter group based on passed filters
        storageRemoteFilterGroup(ioWriteFilterGroup(fileWrite), varLstGet(paramList, 10));

        // Open file
        ioWriteOpen(fileWrite);
        protocolServerResponse(server, NULL);

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
                protocolServerResponse(server, ioFilterGroupResultAll(ioWriteFilterGroup(fileWrite)));
            }
            // Write was aborted so free the file
            else
            {
                ioWriteFree(fileWrite);
                protocolServerResponse(server, NULL);
            }
        }
        while (remaining > 0);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemotePathCreateProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        storageInterfacePathCreateP(
            storageRemoteProtocolLocal.driver, varStr(varLstGet(paramList, 0)), varBool(varLstGet(paramList, 1)),
            varBool(varLstGet(paramList, 2)), (mode_t)varUIntForce(varLstGet(paramList, 3)));

        protocolServerResponse(server, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemotePathRemoveProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolServerResponse(
            server,
            VARBOOL(
                storageInterfacePathRemoveP(
                    storageRemoteProtocolLocal.driver, varStr(varLstGet(paramList, 0)), varBool(varLstGet(paramList, 1)))));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemotePathSyncProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        storageInterfacePathSyncP(storageRemoteProtocolLocal.driver, varStr(varLstGet(paramList, 0)));
        protocolServerResponse(server, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
storageRemoteRemoveProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);
    ASSERT(storageRemoteProtocolLocal.driver != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        storageInterfaceRemoveP(
            storageRemoteProtocolLocal.driver, varStr(varLstGet(paramList, 0)), .errorOnMissing = varBool(varLstGet(paramList, 1)));
        protocolServerResponse(server, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
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
