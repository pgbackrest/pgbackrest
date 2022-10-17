/***********************************************************************************************************************************
Remote Storage Read
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <unistd.h>

#include "common/compress/helper.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/type/convert.h"
#include "common/type/object.h"
#include "storage/remote/protocol.h"
#include "storage/remote/read.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageReadRemote
{
    StorageReadInterface interface;                                 // Interface
    StorageRemote *storage;                                         // Storage that created this object
    StorageRead *read;                                              // Storage read interface

    ProtocolClient *client;                                         // Protocol client for requests
    size_t remaining;                                               // Bytes remaining to be read in block
    Buffer *block;                                                  // Block currently being read
    bool eof;                                                       // Has the file reached eof?

#ifdef DEBUG
    uint64_t protocolReadBytes;                                     // How many bytes were read from the protocol layer?
#endif
} StorageReadRemote;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_REMOTE_TYPE                                                                                      \
    StorageReadRemote *
#define FUNCTION_LOG_STORAGE_READ_REMOTE_FORMAT(value, buffer, bufferSize)                                                         \
    objToLog(value, "StorageReadRemote", buffer, bufferSize)

/***********************************************************************************************************************************
Clear protocol if the entire file is not read or an error occurs before the read is complete. This is required to clear the
protocol state so a subsequent command can succeed.
***********************************************************************************************************************************/
static void
storageReadRemoteFreeResource(THIS_VOID)
{
    THIS(StorageReadRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_REMOTE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Read if eof has not been reached
    if (!this->eof)
    {
        do
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                PackRead *const read = protocolClientDataGet(this->client);
                pckReadNext(read);

                // If binary then discard
                if (pckReadType(read) == pckTypeBin)
                {
                    pckReadBinP(read);
                }
                // Else read is complete so discard the filter list
                else
                {
                    pckReadPackP(read);
                    protocolClientDataEndGet(this->client);

                    this->eof = true;
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
        while (!this->eof);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static size_t
storageReadRemote(THIS_VOID, Buffer *buffer, bool block)
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
                    PackRead *const read = protocolClientDataGet(this->client);
                    pckReadNext(read);

                    // If binary then read the next block
                    if (pckReadType(read) == pckTypeBin)
                    {
                        MEM_CONTEXT_OBJ_BEGIN(this)
                        {
                            this->block = pckReadBinP(read);
                            this->remaining = bufUsed(this->block);
                        }
                        MEM_CONTEXT_OBJ_END();
                    }
                    // Else read is complete and get the filter list
                    else
                    {
                        bufFree(this->block);

                        ioFilterGroupResultAllSet(ioReadFilterGroup(storageReadIo(this->read)), pckReadPackP(read));
                        this->eof = true;

                        protocolClientDataEndGet(this->client);
                    }

#ifdef DEBUG
                    this->protocolReadBytes += this->remaining;
#endif
                }
                MEM_CONTEXT_TEMP_END();
            }

            // Read if not eof
            if (!this->eof)
            {
                // If the buffer can contain all remaining bytes
                if (bufRemains(buffer) >= this->remaining)
                {
                    bufCatSub(buffer, this->block, bufUsed(this->block) - this->remaining, this->remaining);

                    this->remaining = 0;
                    bufFree(this->block);
                    this->block = NULL;
                }
                // Else read what we can
                else
                {
                    size_t remains = bufRemains(buffer);
                    bufCatSub(buffer, this->block, bufUsed(this->block) - this->remaining, remains);
                    this->remaining -= remains;
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
Open the file
***********************************************************************************************************************************/
static bool
storageReadRemoteOpen(THIS_VOID)
{
    THIS(StorageReadRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_REMOTE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If the file is compressible add compression filter on the remote
        if (this->interface.compressible)
        {
            ioFilterGroupAdd(
                ioReadFilterGroup(storageReadIo(this->read)), compressFilter(compressTypeGz, (int)this->interface.compressLevel));
        }

        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_OPEN_READ);
        PackWrite *const param = protocolCommandParam(command);

        pckWriteStrP(param, this->interface.name);
        pckWriteBoolP(param, this->interface.ignoreMissing);
        pckWriteU64P(param, this->interface.offset);

        if (this->interface.limit == NULL)
            pckWriteNullP(param);
        else
            pckWriteU64P(param, varUInt64(this->interface.limit));

        pckWritePackP(param, ioFilterGroupParamAll(ioReadFilterGroup(storageReadIo(this->read))));

        protocolClientCommandPut(this->client, command, false);

        // If the file exists
        result = pckReadBoolP(protocolClientDataGet(this->client));

        if (result)
        {
            // Clear filters since they will be run on the remote side
            ioFilterGroupClear(ioReadFilterGroup(storageReadIo(this->read)));

            // If the file is compressible add decompression filter locally
            if (this->interface.compressible)
                ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(this->read)), decompressFilter(compressTypeGz));

            // Set free callback to ensure the protocol is cleared on a short read
            memContextCallbackSet(objMemContext(this), storageReadRemoteFreeResource, this);
        }
        // Else nothing to do
        else
            protocolClientDataEndGet(this->client);
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

    memContextCallbackClear(objMemContext(this));
    storageReadRemoteFreeResource(this);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
StorageRead *
storageReadRemoteNew(
    StorageRemote *const storage, ProtocolClient *const client, const String *const name, const bool ignoreMissing,
    const bool compressible, const unsigned int compressLevel, const uint64_t offset, const Variant *const limit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, storage);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(BOOL, compressible);
        FUNCTION_LOG_PARAM(UINT, compressLevel);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(client != NULL);
    ASSERT(name != NULL);

    StorageReadRemote *this = NULL;

    OBJ_NEW_BEGIN(StorageReadRemote, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        this = OBJ_NEW_ALLOC();

        *this = (StorageReadRemote)
        {
            .storage = storage,
            .client = client,

            .interface = (StorageReadInterface)
            {
                .type = STORAGE_REMOTE_TYPE,
                .name = strDup(name),
                .compressible = compressible,
                .compressLevel = compressLevel,
                .ignoreMissing = ignoreMissing,
                .offset = offset,
                .limit = varDup(limit),

                .ioInterface = (IoReadInterface)
                {
                    .close = storageReadRemoteClose,
                    .eof = storageReadRemoteEof,
                    .open = storageReadRemoteOpen,
                    .read = storageReadRemote,
                },
            },
        };

        this->read = storageReadNew(this, &this->interface);
    }
    OBJ_NEW_END();

    ASSERT(this != NULL);
    FUNCTION_LOG_RETURN(STORAGE_READ, this->read);
}
