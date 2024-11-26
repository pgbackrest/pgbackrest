/***********************************************************************************************************************************
Remote Storage File write
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/compress/helper.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/write.h"
#include "common/log.h"
#include "common/type/object.h"
#include "storage/remote/protocol.h"
#include "storage/remote/write.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteRemote
{
    StorageWriteInterface interface;                                // Interface
    StorageRemote *storage;                                         // Storage that created this object
    StorageWrite *write;                                            // Storage write interface
    ProtocolClient *client;                                         // Protocol client to make requests with
    ProtocolClientSession *session;                                 // Protocol session for requests

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
    objNameToLog(value, "StorageWriteRemote", buffer, bufferSize)

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
        {
            ioFilterGroupInsert(
                ioWriteFilterGroup(storageWriteIo(this->write)), 0, decompressFilterP(compressTypeLz4, .raw = true));
        }

        PackWrite *const param = protocolPackNew();

        pckWriteStrP(param, this->interface.name);
        pckWriteModeP(param, this->interface.modeFile);
        pckWriteModeP(param, this->interface.modePath);
        pckWriteStrP(param, this->interface.user);
        pckWriteStrP(param, this->interface.group);
        pckWriteTimeP(param, this->interface.timeModified);
        pckWriteBoolP(param, this->interface.createPath);
        pckWriteBoolP(param, this->interface.syncFile);
        pckWriteBoolP(param, this->interface.syncPath);
        pckWriteBoolP(param, this->interface.atomic);
        pckWritePackP(param, ioFilterGroupParamAll(ioWriteFilterGroup(storageWriteIo(this->write))));

        protocolClientSessionOpenP(this->session, .param = param);

        // Clear filters since they will be run on the remote side
        ioFilterGroupClear(ioWriteFilterGroup(storageWriteIo(this->write)));

        // If the file is compressible add compression filter locally
        if (this->interface.compressible)
        {
            ioFilterGroupAdd(
                ioWriteFilterGroup(storageWriteIo(this->write)),
                compressFilterP(compressTypeLz4, (int)this->interface.compressLevel, .raw = true));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to the file
***********************************************************************************************************************************/
static void
storageWriteRemote(THIS_VOID, const Buffer *const buffer)
{
    THIS(StorageWriteRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_REMOTE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (protocolClientSessionQueued(this->session))
            protocolClientSessionResponse(this->session);

        PackWrite *const param = pckWriteNewP(.size = PROTOCOL_PACK_DEFAULT_SIZE + bufUsed(buffer));
        pckWriteBinP(param, buffer);

        protocolClientSessionRequestAsyncP(this->session, .param = param);
    }
    MEM_CONTEXT_TEMP_END();

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
        MEM_CONTEXT_TEMP_BEGIN()
        {
            if (protocolClientSessionQueued(this->session))
                protocolClientSessionResponse(this->session);

            ioFilterGroupResultAllSet(
                ioWriteFilterGroup(storageWriteIo(this->write)), pckReadPackP(protocolClientSessionClose(this->session)));
        }
        MEM_CONTEXT_TEMP_END();

        this->client = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN StorageWrite *
storageWriteRemoteNew(
    StorageRemote *const storage, ProtocolClient *const client, const String *const name, const mode_t modeFile,
    const mode_t modePath, const String *const user, const String *const group, const time_t timeModified, const bool createPath,
    const bool syncFile, const bool syncPath, const bool atomic, const bool compressible, const unsigned int compressLevel)
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

    OBJ_NEW_BEGIN(StorageWriteRemote, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (StorageWriteRemote)
        {
            .storage = storage,
            .client = client,
            .session = protocolClientSessionNewP(client, PROTOCOL_COMMAND_STORAGE_WRITE, .async = true),

            .interface = (StorageWriteInterface)
            {
                .type = STORAGE_REMOTE_TYPE,
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
                .truncate = true,
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

        this->write = storageWriteNew(OBJ_NAME(this, StorageWrite::StorageWriteRemote), &this->interface);
    }
    OBJ_NEW_END();

    ASSERT(this != NULL);
    FUNCTION_LOG_RETURN(STORAGE_WRITE, this->write);
}
