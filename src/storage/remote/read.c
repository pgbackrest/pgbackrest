/***********************************************************************************************************************************
Remote Storage Read
***********************************************************************************************************************************/
#include <build.h>

#include <fcntl.h>
#include <unistd.h>

#include "common/compress/helper.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/type/convert.h"
#include "common/type/object.h"
#include "storage/read.h"
#include "storage/remote/protocol.h"
#include "storage/remote/read.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageReadRemote
{
    const StorageReadInterface *interface;                          // Interface
    StorageRemote *storage;                                         // Storage that created this object
    IoFilterGroup *filterGroup;                                     // Filter group
    const String *name;                                             // File name
    uint64_t offset;                                                // Read offset
    const Variant *limit;                                           // Read limit (NULL for no limit)
    const String *versionId;                                        // Version id (NULL for most recent)

    ProtocolClient *client;                                         // Protocol client for requests
    ProtocolClientSession *session;                                 // Protocol session for requests
    size_t remaining;                                               // Bytes remaining to be read in block
    Buffer *block;                                                  // Block currently being read
    bool eof;                                                       // Has the file reached eof?
    bool eofFound;                                                  // Eof found but a block is remaining to be read
    bool compressible;                                              // Is this file compressible?
    unsigned int compressLevel;                                     // Level to use for compression

#ifdef DEBUG
    uint64_t protocolReadBytes;                                     // How many bytes were read from the protocol layer?
#endif
};

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static void
storageReadRemoteInternal(StorageReadRemote *const this, PackRead *const packRead)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_REMOTE, this);
        FUNCTION_LOG_PARAM(PACK_READ, packRead);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(packRead != NULL);

    FUNCTION_AUDIT_HELPER();

    // If not done then read the next block
    if (!pckReadBoolP(packRead))
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            this->block = pckReadBinP(packRead);
            this->remaining = bufUsed(this->block);
        }
        MEM_CONTEXT_OBJ_END();
    }

    // If eof then get results
    if (protocolClientSessionClosed(this->session))
    {
        ioFilterGroupResultAllSet(this->filterGroup, pckReadPackP(packRead));

        this->eofFound = true;

        if (this->remaining == 0)
            this->eof = true;
    }

#ifdef DEBUG
    this->protocolReadBytes += this->remaining;
#endif

    FUNCTION_LOG_RETURN_VOID();
}

static size_t
storageReadRemote(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(StorageReadRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_REMOTE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    size_t result = 0;

    // Read if eof has not been reached
    if (!this->eof)
    {
        do
        {
            // If no bytes remaining then read a new block
            if (this->remaining == 0)
            {
                MEM_CONTEXT_TEMP_BEGIN()
                {
                    if (!protocolClientSessionQueued(this->session))
                        protocolClientSessionRequestAsyncP(this->session);

                    storageReadRemoteInternal(this, protocolClientSessionResponse(this->session));

                    if (!this->eofFound)
                        protocolClientSessionRequestAsyncP(this->session);
                }
                MEM_CONTEXT_TEMP_END();
            }

            // Read if not eof
            if (!this->eof)
            {
                // Copy as much as possible into the output buffer
                const size_t remains = this->remaining < bufRemains(buffer) ? this->remaining : bufRemains(buffer);

                bufCatSub(buffer, this->block, bufUsed(this->block) - this->remaining, remains);

                result += remains;
                this->remaining -= remains;

                // If there is no more to copy from the block buffer then free it
                if (this->remaining == 0)
                {
                    bufFree(this->block);
                    this->block = NULL;

                    if (this->eofFound)
                        this->eof = true;
                }
            }
        }
        while (!this->eof && !bufFull(buffer));
    }

    FUNCTION_LOG_RETURN(SIZE, result);
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
static bool
storageReadRemoteEof(THIS_VOID)
{
    THIS(StorageReadRemote);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_REMOTE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->eof);
}

/***********************************************************************************************************************************
Set filter group
***********************************************************************************************************************************/
static void
storageReadRemoteFilterGroup(THIS_VOID, IoFilterGroup *const filterGroup)
{
    THIS(StorageReadRemote);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_REMOTE, this);
        FUNCTION_TEST_PARAM(IO_FILTER_GROUP, filterGroup);
    FUNCTION_TEST_END();

    this->filterGroup = filterGroup;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadRemoteOpen(THIS_VOID, const bool ignoreMissing)
{
    THIS(StorageReadRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_REMOTE, this);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If the file is compressible add compression filter on the remote
        if (this->compressible)
            ioFilterGroupAdd(this->filterGroup, compressFilterP(compressTypeLz4, (int)this->compressLevel, .raw = true));

        PackWrite *const param = protocolPackNew();

        pckWriteStrP(param, this->name);
        pckWriteBoolP(param, ignoreMissing);
        pckWriteU64P(param, this->offset);

        if (this->limit == NULL)
            pckWriteNullP(param);
        else
            pckWriteU64P(param, varUInt64(this->limit));

        pckWriteStrP(param, this->versionId);
        pckWritePackP(param, ioFilterGroupParamAll(this->filterGroup));

        // If the file exists
        PackRead *const packRead = protocolClientSessionOpenP(this->session, .param = param);
        result = pckReadBoolP(packRead);

        if (result)
        {
            // Clear filters since they will be run on the remote side
            ioFilterGroupClear(this->filterGroup);

            // If the file is compressible add decompression filter locally
            if (this->compressible)
                ioFilterGroupAdd(this->filterGroup, decompressFilterP(compressTypeLz4, .raw = true));

            // Read the first block or eof
            storageReadRemoteInternal(this, packRead);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Close the file and read any remaining data. It is possible that all data has been read but if the amount of data is exactly
divisible by the buffer size then the eof may not have been received.
***********************************************************************************************************************************/
static void
storageReadRemoteClose(THIS_VOID)
{
    THIS(StorageReadRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_REMOTE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    protocolClientSessionCancel(this->session);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageReadInterface storageReadRemoteInterface =
{
    .close = storageReadRemoteClose,
    .eof = storageReadRemoteEof,
    .filterGroup = storageReadRemoteFilterGroup,
    .open = storageReadRemoteOpen,
    .read = storageReadRemote,
};

FN_EXTERN StorageReadRemote *
storageReadRemoteNew(
    StorageRemote *const storage, ProtocolClient *const client, const String *const name, const bool compressible,
    const unsigned int compressLevel, const uint64_t offset, const Variant *const limit, const String *const versionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, storage);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, compressible);
        FUNCTION_LOG_PARAM(UINT, compressLevel);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
        FUNCTION_LOG_PARAM(STRING, versionId);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(client != NULL);
    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(StorageReadRemote, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (StorageReadRemote)
        {
            .interface = &storageReadRemoteInterface,
            .storage = storage,
            .client = client,
            .session = protocolClientSessionNewP(client, PROTOCOL_COMMAND_STORAGE_READ, .async = true),
            .name = strDup(name),
            .compressible = compressible,
            .compressLevel = compressLevel,
            .offset = offset,
            .limit = varDup(limit),
            .versionId = strDup(versionId),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ_REMOTE, this);
}
