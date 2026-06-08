/***********************************************************************************************************************************
Remote Storage File write
***********************************************************************************************************************************/
#include <build.h>

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
struct StorageWriteRemote
{
    const StorageWriteInterface *interface;                         // Interface
    StorageRemote *storage;                                         // Storage that created this object
    ProtocolClient *client;                                         // Protocol client to make requests with
    ProtocolClientSession *session;                                 // Protocol session for requests
    IoFilterGroup *filterGroup;                                     // Filter group
    const String *name;                                             // File name
    mode_t modeFile;                                                // File mode
    mode_t modePath;                                                // Path mode
    const String *user;                                             // User name
    const String *group;                                            // Group name
    time_t timeModified;                                            // Time modified
    bool createPath;                                                // Create path
    bool syncFile;                                                  // Sync file
    bool syncPath;                                                  // Sync path
    bool atomic;                                                    // Atomic write
    bool compressible;                                              // Is this file compressible?
    unsigned int compressLevel;                                     // Level to use for compression

#ifdef DEBUG
    uint64_t protocolWriteBytes;                                    // How many bytes were written to the protocol layer?
#endif
};

/***********************************************************************************************************************************
Set filter group
***********************************************************************************************************************************/
static void
storageWriteRemoteFilterGroup(THIS_VOID, IoFilterGroup *const filterGroup)
{
    THIS(StorageWriteRemote);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE_REMOTE, this);
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, filterGroup);
    FUNCTION_TEST_END();

    this->filterGroup = filterGroup;

    FUNCTION_TEST_RETURN_VOID();
}

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
        if (this->compressible)
        {
            ioFilterGroupInsert(
                this->filterGroup, 0, decompressFilterP(compressTypeLz4, .raw = true));
        }

        PackWrite *const param = protocolPackNew();

        pckWriteStrP(param, this->name);
        pckWriteModeP(param, this->modeFile);
        pckWriteModeP(param, this->modePath);
        pckWriteStrP(param, this->user);
        pckWriteStrP(param, this->group);
        pckWriteTimeP(param, this->timeModified);
        pckWriteBoolP(param, this->createPath);
        pckWriteBoolP(param, this->syncFile);
        pckWriteBoolP(param, this->syncPath);
        pckWriteBoolP(param, this->atomic);
        pckWritePackP(param, ioFilterGroupParamAll(this->filterGroup));

        protocolClientSessionOpenP(this->session, .param = param);

        // Clear filters since they will be run on the remote side
        ioFilterGroupClear(this->filterGroup);

        // If the file is compressible add compression filter locally
        if (this->compressible)
        {
            ioFilterGroupAdd(
                this->filterGroup,
                compressFilterP(compressTypeLz4, (int)this->compressLevel, .raw = true));
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
                this->filterGroup, pckReadPackP(protocolClientSessionClose(this->session)));
        }
        MEM_CONTEXT_TEMP_END();

        this->client = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageWriteInterface storageWriteRemoteInterface =
{
    .close = storageWriteRemoteClose,
    .filterGroup = storageWriteRemoteFilterGroup,
    .open = storageWriteRemoteOpen,
    .write = storageWriteRemote,
};

FN_EXTERN StorageWriteRemote *
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
            .interface = &storageWriteRemoteInterface,
            .storage = storage,
            .client = client,
            .session = protocolClientSessionNewP(client, PROTOCOL_COMMAND_STORAGE_WRITE, .async = true),
            .name = strDup(name),
            .modeFile = modeFile,
            .modePath = modePath,
            .user = strDup(user),
            .group = strDup(group),
            .timeModified = timeModified,
            .createPath = createPath,
            .syncFile = syncFile,
            .syncPath = syncPath,
            .atomic = atomic,
            .compressible = compressible,
            .compressLevel = compressLevel,
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE_REMOTE, this);
}
