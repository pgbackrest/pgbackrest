/***********************************************************************************************************************************
Remote Storage Read Multi

Forward the entire request list to the remote in a single session open so the remote can run the multi read against its locally
attached storage, where prefetch and read over are most effective, and stream the result back as one continuous stream.
***********************************************************************************************************************************/
#include <build.h>

#include "common/compress/helper.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "storage/remote/protocol.h"
#include "storage/remote/read.h"
#include "storage/remote/readMulti.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageReadMultiRemote
{
    const StorageReadMultiInterface *interface;                     // Interface (must be first member)
    StorageRemote *storage;                                         // Storage that created this object
    IoFilterGroup *filterGroup;                                     // Filter group

    List *requestList;                                              // List of read requests
    unsigned int compressLevel;                                     // Level to use for compression
    bool compressible;                                              // Is any file compressible?

    StorageReadRemoteStream stream;                                 // Protocol stream state
};

/***********************************************************************************************************************************
Structure to store read requests until open
***********************************************************************************************************************************/
typedef struct StorageReadMultiRemoteRequest
{
    const String *file;                                             // File to read
    bool compressible;                                              // Is the file compressible?
    uint64_t offset;                                                // Where to start reading in the file
    const Variant *limit;                                           // Limit bytes to read from the file (NULL for no limit)
    const String *versionId;                                        // Version to read (NULL for current version)
} StorageReadMultiRemoteRequest;

/***********************************************************************************************************************************
Add a file to the read
***********************************************************************************************************************************/
static void
storageReadMultiRemoteAdd(THIS_VOID, const String *const file, const StorageReadMultiAddParam param)
{
    THIS(StorageReadMultiRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI_REMOTE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, param.compressible);
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
        FUNCTION_LOG_PARAM(STRING, param.versionId);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    MEM_CONTEXT_OBJ_BEGIN(this->requestList)
    {
        const StorageReadMultiRemoteRequest storageReadMultiRemoteRequest =
        {
            .file = strDup(file),
            .compressible = param.compressible,
            .offset = param.offset,
            .limit = varDup(param.limit),
            .versionId = strDup(param.versionId),
        };

        lstAdd(this->requestList, &storageReadMultiRemoteRequest);
    }
    MEM_CONTEXT_OBJ_END();

    // If any file is compressible then the stream will be compressed for transport
    if (this->compressLevel > 0 && param.compressible)
        this->compressible = true;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Set filter group
***********************************************************************************************************************************/
static void
storageReadMultiRemoteFilterGroup(THIS_VOID, IoFilterGroup *const filterGroup)
{
    THIS(StorageReadMultiRemote);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_MULTI_REMOTE, this);
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, filterGroup);
    FUNCTION_TEST_END();

    this->filterGroup = filterGroup;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Open the read by sending the request list to the remote
***********************************************************************************************************************************/
static bool
storageReadMultiRemoteOpen(THIS_VOID)
{
    THIS(StorageReadMultiRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI_REMOTE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If the stream is compressible add compression filter on the remote
        if (this->compressible)
            ioFilterGroupAdd(this->filterGroup, compressFilterP(compressTypeLz4, (int)this->compressLevel, .raw = true));

        PackWrite *const param = protocolPackNew();

        pckWritePackP(param, ioFilterGroupParamAll(this->filterGroup));

        // Add requests
        for (unsigned int requestIdx = 0; requestIdx < lstSize(this->requestList); requestIdx++)
        {
            const StorageReadMultiRemoteRequest *const request = lstGet(this->requestList, requestIdx);

            pckWriteStrP(param, request->file);
            pckWriteBoolP(param, request->compressible);
            pckWriteU64P(param, request->offset);

            if (request->limit == NULL)
                pckWriteNullP(param);
            else
                pckWriteU64P(param, varUInt64(request->limit));

            pckWriteStrP(param, request->versionId);
        }

        PackRead *const packRead = protocolClientSessionOpenP(this->stream.session, .param = param);

        // Clear filters since they will be run on the remote side
        ioFilterGroupClear(this->filterGroup);

        // If the stream is compressible add decompression filter locally
        if (this->compressible)
            ioFilterGroupAdd(this->filterGroup, decompressFilterP(compressTypeLz4, .raw = true));

        // Read the first chunk or eof
        storageReadRemoteStreamInternal(&this->stream, this->filterGroup, packRead);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, true);
}

/***********************************************************************************************************************************
Read from the remote stream
***********************************************************************************************************************************/
static size_t
storageReadMultiRemote(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(StorageReadMultiRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI_REMOTE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    FUNCTION_LOG_RETURN(SIZE, storageReadRemoteStreamRead(&this->stream, this->filterGroup, buffer));
}

/***********************************************************************************************************************************
Has read reached EOF?
***********************************************************************************************************************************/
static bool
storageReadMultiRemoteEof(THIS_VOID)
{
    THIS(StorageReadMultiRemote);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_MULTI_REMOTE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->stream.eof);
}

/***********************************************************************************************************************************
Close the read
***********************************************************************************************************************************/
static void
storageReadMultiRemoteClose(THIS_VOID)
{
    THIS(StorageReadMultiRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_MULTI_REMOTE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    protocolClientSessionCancel(this->stream.session);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageReadMultiInterface storageReadMultiRemoteInterface =
{
    .add = storageReadMultiRemoteAdd,
    .close = storageReadMultiRemoteClose,
    .eof = storageReadMultiRemoteEof,
    .filterGroup = storageReadMultiRemoteFilterGroup,
    .open = storageReadMultiRemoteOpen,
    .read = storageReadMultiRemote,
};

FN_EXTERN StorageReadMultiRemote *
storageReadMultiRemoteNew(StorageRemote *const storage, ProtocolClient *const client, const unsigned int compressLevel)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, storage);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
        FUNCTION_LOG_PARAM(UINT, compressLevel);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(client != NULL);

    OBJ_NEW_BEGIN(StorageReadMultiRemote, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (StorageReadMultiRemote)
        {
            .interface = &storageReadMultiRemoteInterface,
            .storage = storage,
            .requestList = lstNewP(sizeof(StorageReadMultiRemoteRequest)),
            .compressLevel = compressLevel,
            .stream =
            {
                .memContext = memContextCurrent(),
                .session = protocolClientSessionNewP(client, PROTOCOL_COMMAND_STORAGE_READ_MULTI, .async = true),
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ_MULTI_REMOTE, this);
}
