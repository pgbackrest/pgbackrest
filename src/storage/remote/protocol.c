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
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_FEATURE_STR,                 PROTOCOL_COMMAND_STORAGE_FEATURE);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_INFO_STR,                    PROTOCOL_COMMAND_STORAGE_INFO);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_INFO_LIST_STR,               PROTOCOL_COMMAND_STORAGE_INFO_LIST);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR,               PROTOCOL_COMMAND_STORAGE_OPEN_READ);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_OPEN_WRITE_STR,              PROTOCOL_COMMAND_STORAGE_OPEN_WRITE);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_PATH_CREATE_STR,             PROTOCOL_COMMAND_STORAGE_PATH_CREATE);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_PATH_REMOVE_STR,             PROTOCOL_COMMAND_STORAGE_PATH_REMOVE);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_PATH_SYNC_STR,               PROTOCOL_COMMAND_STORAGE_PATH_SYNC);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_REMOVE_STR,                  PROTOCOL_COMMAND_STORAGE_REMOVE);

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
    RegExp *blockRegExp;                                            // Regular expression to check block messages
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
typedef struct StorageRemoteProtocolInfoListCallbackData
{
    MemContext *memContext;                                         // Mem context used to store values from last call
    PackWrite *write;                                               // Pack to write into protocol
    time_t timeModifiedLast;                                        // timeModified from last call
    mode_t modeLast;                                                // mode from last call
    uid_t userIdLast;                                               // userId from last call
    gid_t groupIdLast;                                              // groupId from last call
    String *user;                                                   // user from last call
    String *group;                                                  // group from last call
} StorageRemoteProtocolInfoListCallbackData;

// Helper to write storage info into the protocol. This function is not called unless the info exists so no need to write exists or
// check for level == storageInfoLevelExists.
//
// Fields that do not change from one call to the next are omitted to save bandwidth.
static void
storageRemoteInfoWrite(StorageRemoteProtocolInfoListCallbackData *data, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(info != NULL);

    // Write type and time
    pckWriteU32P(data->write, info->type);
    pckWriteTimeP(data->write, info->timeModified - data->timeModifiedLast);

    // Write size for files
    if (info->type == storageTypeFile)
        pckWriteU64P(data->write, info->size);

    // Write fields needed for detail level
    if (info->level >= storageInfoLevelDetail)
    {
        // Write mode
        pckWriteU32P(data->write, info->mode, .defaultValue = data->modeLast);

        // Write user id/name
        pckWriteU32P(data->write, info->userId, .defaultValue = data->userIdLast);

        if (info->user == NULL)
            pckWriteBoolP(data->write, true);
        else
        {
            pckWriteNullP(data->write);
            pckWriteStrP(data->write, info->user, .defaultValue = data->user);
        }

        // Write group id/name
        pckWriteU32P(data->write, info->groupId, .defaultValue = data->groupIdLast);

        if (info->group == NULL)
            pckWriteBoolP(data->write, true);
        else
        {
            pckWriteNullP(data->write);
            pckWriteStrP(data->write, info->group, .defaultValue = data->group);
        }

        // Write link destination
        if (info->type == storageTypeLink)
            pckWriteStrP(data->write, info->linkDestination);
    }

    // Store defaults to use for the next call. If memContext is NULL this function is only being called one time so there is no
    // point in storing defaults.
    if (data->memContext != NULL)
    {
        data->timeModifiedLast = info->timeModified;
        data->modeLast = info->mode;
        data->userIdLast = info->userId;
        data->groupIdLast = info->groupId;

        if (!strEq(info->user, data->user) && info->user != NULL)
        {
            strFree(data->user);

            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->user = strDup(info->user);
            }
            MEM_CONTEXT_END();
        }

        if (!strEq(info->group, data->group) && info->group != NULL)
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

    pckWriteObjBeginP(data->write);
    pckWriteStrP(data->write, info->name);
    storageRemoteInfoWrite(data, info);
    pckWriteObjEndP(data->write);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
storageRemoteProtocol(const String *command, const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);

    // Determine which storage should be used
    const Storage *storage = protocolStorageTypeEnum(
        cfgOptionStr(cfgOptRemoteType)) == protocolStorageTypeRepo ? storageRepo() : storagePg();
    StorageInterface interface = storageInterface(storage);
    void *driver = storageDriver(storage);

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, PROTOCOL_COMMAND_STORAGE_FEATURE_STR))
        {
            protocolServerWriteLine(server, jsonFromStr(storagePathP(storage, NULL)));
            protocolServerWriteLine(server, jsonFromUInt64(interface.feature));

            protocolServerResponse(server, NULL);
        }
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_INFO_STR))
        {
            StorageInfo info = storageInterfaceInfoP(
                driver, varStr(varLstGet(paramList, 0)), (StorageInfoLevel)varUIntForce(varLstGet(paramList, 1)),
                .followLink = varBool(varLstGet(paramList, 2)));

            PackWrite *write = pckWriteNew(protocolServerIoWrite(server));
            pckWriteBoolP(write, info.exists, .defaultWrite = true);

            if (info.exists)
            {
                pckWriteObjBeginP(write);
                storageRemoteInfoWrite(&(StorageRemoteProtocolInfoListCallbackData){.write = write}, &info);
                pckWriteObjEndP(write);
            }

            pckWriteEndP(write);
            ioWriteFlush(protocolServerIoWrite(server));
        }
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_INFO_LIST_STR))
        {
            PackWrite *write = pckWriteNew(protocolServerIoWrite(server));
            pckWriteArrayBeginP(write);

            StorageRemoteProtocolInfoListCallbackData data = {.write = write, .memContext = memContextCurrent()};

            bool result = storageInterfaceInfoListP(
                driver, varStr(varLstGet(paramList, 0)), (StorageInfoLevel)varUIntForce(varLstGet(paramList, 1)),
                storageRemoteProtocolInfoListCallback, &data);

            pckWriteArrayEndP(write);
            pckWriteBoolP(write, result, .defaultWrite = true);
            pckWriteEndP(write);
            ioWriteFlush(protocolServerIoWrite(server));
        }
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR))
        {
            // Create the read object
            IoRead *fileRead = storageReadIo(
                storageInterfaceNewReadP(
                    driver, varStr(varLstGet(paramList, 0)), varBool(varLstGet(paramList, 1)), .limit = varLstGet(paramList, 2)));

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

                    if (bufUsed(buffer) > 0)
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
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_OPEN_WRITE_STR))
        {
            // Create the write object
            IoWrite *fileWrite = storageWriteIo(
                storageInterfaceNewWriteP(
                    driver, varStr(varLstGet(paramList, 0)), .modeFile = varUIntForce(varLstGet(paramList, 1)),
                    .modePath = varUIntForce(varLstGet(paramList, 2)), .user = varStr(varLstGet(paramList, 3)),
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
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_PATH_CREATE_STR))
        {
            storageInterfacePathCreateP(
                driver, varStr(varLstGet(paramList, 0)), varBool(varLstGet(paramList, 1)), varBool(varLstGet(paramList, 2)),
                varUIntForce(varLstGet(paramList, 3)));

            protocolServerResponse(server, NULL);
        }
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_PATH_REMOVE_STR))
        {
            protocolServerResponse(server,
                VARBOOL(storageInterfacePathRemoveP(driver, varStr(varLstGet(paramList, 0)), varBool(varLstGet(paramList, 1)))));
        }
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_PATH_SYNC_STR))
        {
            storageInterfacePathSyncP(driver, varStr(varLstGet(paramList, 0)));

            protocolServerResponse(server, NULL);
        }
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_REMOVE_STR))
        {
            storageInterfaceRemoveP(driver, varStr(varLstGet(paramList, 0)), .errorOnMissing = varBool(varLstGet(paramList, 1)));

            protocolServerResponse(server, NULL);
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}

/**********************************************************************************************************************************/
ssize_t
storageRemoteProtocolBlockSize(const String *message)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, message);
    FUNCTION_LOG_END();

    ASSERT(message != NULL);

    // Create block regular expression if it has not been created yet
    if (storageRemoteProtocolLocal.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN("StorageRemoteFileReadLocal")
            {
                storageRemoteProtocolLocal.memContext = memContextCurrent();
                storageRemoteProtocolLocal.blockRegExp = regExpNew(BLOCK_REG_EXP_STR);
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }

    // Validate the header block size message
    if (!regExpMatch(storageRemoteProtocolLocal.blockRegExp, message))
        THROW_FMT(ProtocolError, "'%s' is not a valid block size message", strZ(message));

    FUNCTION_LOG_RETURN(SSIZE, (ssize_t)cvtZToInt(strZ(message) + sizeof(PROTOCOL_BLOCK_HEADER) - 1));
}
