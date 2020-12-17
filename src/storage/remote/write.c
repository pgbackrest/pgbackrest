/***********************************************************************************************************************************
Remote Storage File write
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/compress/helper.h"
#include "common/debug.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "common/type/object.h"
#include "storage/remote/protocol.h"
#include "storage/remote/write.h"
#include "storage/write.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define STORAGE_WRITE_REMOTE_TYPE                                   StorageWriteRemote
#define STORAGE_WRITE_REMOTE_PREFIX                                 storageWriteRemote

typedef struct StorageWriteRemote
{
    MemContext *memContext;                                         // Object mem context
    StorageWriteInterface interface;                                // Interface
    StorageRemote *storage;                                         // Storage that created this object
    StorageWrite *write;                                            // Storage write interface
    ProtocolClient *client;                                         // Protocol client to make requests with

#ifdef DEBUG
    uint64_t protocolWriteBytes;                                    // How many bytes were written to the protocol layer?
#endif
} StorageWriteRemote;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_REMOTE_TYPE                                                                                     \
    StorageWriteRemote *
#define FUNCTION_LOG_STORAGE_WRITE_REMOTE_FORMAT(value, buffer, bufferSize)                                                        \
    objToLog(value, "StorageWriteRemote", buffer, bufferSize)

/***********************************************************************************************************************************
Close file on the remote
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(STORAGE_WRITE_REMOTE, LOG, logLevelTrace)
{
    ioWriteLine(protocolClientIoWrite(this->client), BUFSTRDEF(PROTOCOL_BLOCK_HEADER "-1"));
    ioWriteFlush(protocolClientIoWrite(this->client));
    protocolClientReadOutput(this->client, false);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static void
storageWriteRemoteOpen(THIS_VOID)
{
    THIS(StorageWriteRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_REMOTE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If the file is compressible add decompression filter on the remote
        if (this->interface.compressible)
            ioFilterGroupInsert(ioWriteFilterGroup(storageWriteIo(this->write)), 0, decompressFilter(compressTypeGz));

        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_OPEN_WRITE_STR);
        PackWrite *param = protocolCommandParam(command);

        pckWriteStrP(param, this->interface.name);
        pckWriteU32P(param, this->interface.modeFile);
        pckWriteU32P(param, this->interface.modePath);
        pckWriteStrP(param, this->interface.user);
        pckWriteStrP(param, this->interface.group);
        pckWriteTimeP(param, this->interface.timeModified);
        pckWriteBoolP(param, this->interface.createPath);
        pckWriteBoolP(param, this->interface.syncFile);
        pckWriteBoolP(param, this->interface.syncPath);
        pckWriteBoolP(param, this->interface.atomic);
        pckWriteStrP(param, jsonFromVar(ioFilterGroupParamAll(ioWriteFilterGroup(storageWriteIo(this->write)))));

        protocolClientExecute(this->client, command, false);

        // Clear filters since they will be run on the remote side
        ioFilterGroupClear(ioWriteFilterGroup(storageWriteIo(this->write)));

        // If the file is compressible add compression filter locally
        if (this->interface.compressible)
        {
            ioFilterGroupAdd(
                ioWriteFilterGroup(storageWriteIo(this->write)),
                compressFilter(compressTypeGz, (int)this->interface.compressLevel));
        }

        // Set free callback to ensure remote file is freed
        memContextCallbackSet(this->memContext, storageWriteRemoteFreeResource, this);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to the file
***********************************************************************************************************************************/
static void
storageWriteRemote(THIS_VOID, const Buffer *buffer)
{
    THIS(StorageWriteRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_REMOTE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    ioWriteStrLine(protocolClientIoWrite(this->client), strNewFmt(PROTOCOL_BLOCK_HEADER "%zu", bufUsed(buffer)));
    ioWrite(protocolClientIoWrite(this->client), buffer);
    ioWriteFlush(protocolClientIoWrite(this->client));

#ifdef DEBUG
    this->protocolWriteBytes += bufUsed(buffer);
#endif

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
storageWriteRemoteClose(THIS_VOID)
{
    THIS(StorageWriteRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_REMOTE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Close if the file has not already been closed
    if (this->client != NULL)
    {
        ioWriteLine(protocolClientIoWrite(this->client), BUFSTRDEF(PROTOCOL_BLOCK_HEADER "0"));
        ioWriteFlush(protocolClientIoWrite(this->client));
        ioFilterGroupResultAllSet(ioWriteFilterGroup(storageWriteIo(this->write)), protocolClientReadOutput(this->client, true));
        this->client = NULL;

        memContextCallbackClear(this->memContext);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
StorageWrite *
storageWriteRemoteNew(
    StorageRemote *storage, ProtocolClient *client, const String *name, mode_t modeFile, mode_t modePath, const String *user,
    const String *group, time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic, bool compressible,
    unsigned int compressLevel)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, storage);
        FUNCTION_LOG_PARAM(STRING, name);
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
        FUNCTION_LOG_PARAM(UINT, compressLevel);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(client != NULL);
    ASSERT(name != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    StorageWriteRemote *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageWriteRemote")
    {
        this = memNew(sizeof(StorageWriteRemote));

        *this = (StorageWriteRemote)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .storage = storage,
            .client = client,

            .interface = (StorageWriteInterface)
            {
                .type = STORAGE_REMOTE_TYPE_STR,
                .name = strDup(name),
                .atomic = atomic,
                .compressible = compressible,
                .compressLevel = compressLevel,
                .createPath = createPath,
                .group = strDup(group),
                .modeFile = modeFile,
                .modePath = modePath,
                .syncFile = syncFile,
                .syncPath = syncPath,
                .user = strDup(user),
                .timeModified = timeModified,

                .ioInterface = (IoWriteInterface)
                {
                    .close = storageWriteRemoteClose,
                    .open = storageWriteRemoteOpen,
                    .write = storageWriteRemote,
                },
            },
        };

        this->write = storageWriteNew(this, &this->interface);
    }
    MEM_CONTEXT_NEW_END();

    ASSERT(this != NULL);
    FUNCTION_LOG_RETURN(STORAGE_WRITE, this->write);
}
