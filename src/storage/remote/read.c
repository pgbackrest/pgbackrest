/***********************************************************************************************************************************
Remote Storage Read
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <unistd.h>

#include "common/compress/helper.h"
#include "common/debug.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/convert.h"
#include "common/type/json.h"
#include "common/type/object.h"
#include "storage/remote/protocol.h"
#include "storage/remote/read.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageReadRemote
{
    MemContext *memContext;                                         // Object mem context
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
        PackWrite *param = protocolCommandParam(command);

        pckWriteStrP(param, this->interface.name);
        pckWriteBoolP(param, this->interface.ignoreMissing);
        pckWriteStrP(param, jsonFromVar(this->interface.limit));
        pckWriteStrP(param, jsonFromVar(ioFilterGroupParamAll(ioReadFilterGroup(storageReadIo(this->read)))));

        protocolClientWriteCommand(this->client, command);

        // If the file exists
        result = pckReadBoolP(protocolClientDataGet(this->client));

        if (result)
        {
            // Clear filters since they will be run on the remote side
            ioFilterGroupClear(ioReadFilterGroup(storageReadIo(this->read)));

            // If the file is compressible add decompression filter locally
            if (this->interface.compressible)
                ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(this->read)), decompressFilter(compressTypeGz));
        }
        // Else nothing to do
        else
            protocolClientResultGet(this->client);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
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

                    if (pckReadType(read) == pckTypeBin)
                    {
                        MEM_CONTEXT_BEGIN(this->memContext)
                        {
                            this->block = pckReadBinP(read);
                            this->remaining = bufUsed(this->block);
                        }
                        MEM_CONTEXT_END();
                    }
                    else
                    {
                        bufFree(this->block);

                        ioFilterGroupResultAllSet(ioReadFilterGroup(storageReadIo(this->read)), jsonToVar(pckReadStrP(read)));
                        this->eof = true;

                        protocolClientResultGet(this->client);
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

    FUNCTION_TEST_RETURN(this->eof);
}

/**********************************************************************************************************************************/
StorageRead *
storageReadRemoteNew(
    StorageRemote *storage, ProtocolClient *client, const String *name, bool ignoreMissing, bool compressible,
    unsigned int compressLevel, const Variant *limit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, storage);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(BOOL, compressible);
        FUNCTION_LOG_PARAM(UINT, compressLevel);
        FUNCTION_LOG_PARAM(VARIANT, limit);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(client != NULL);
    ASSERT(name != NULL);

    StorageReadRemote *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageReadRemote")
    {
        this = memNew(sizeof(StorageReadRemote));

        *this = (StorageReadRemote)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .storage = storage,
            .client = client,

            .interface = (StorageReadInterface)
            {
                .type = STORAGE_REMOTE_TYPE,
                .name = strDup(name),
                .compressible = compressible,
                .compressLevel = compressLevel,
                .ignoreMissing = ignoreMissing,
                .limit = varDup(limit),

                .ioInterface = (IoReadInterface)
                {
                    .eof = storageReadRemoteEof,
                    .open = storageReadRemoteOpen,
                    .read = storageReadRemote,
                },
            },
        };

        this->read = storageReadNew(this, &this->interface);
    }
    MEM_CONTEXT_NEW_END();

    ASSERT(this != NULL);
    FUNCTION_LOG_RETURN(STORAGE_READ, this->read);
}
