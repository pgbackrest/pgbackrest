/***********************************************************************************************************************************
Remote Storage File Read Driver
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/read.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/type/convert.h"
#include "storage/driver/remote/fileRead.h"
#include "storage/driver/remote/protocol.h"
#include "storage/fileRead.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageFileReadDriverRemote
{
    MemContext *memContext;                                         // Object mem context
    StorageFileReadInterface interface;                             // Driver interface
    StorageDriverRemote *storage;                                   // Storage that created this object

    ProtocolClient *client;                                         // Protocol client for requests
    size_t remaining;                                               // Bytes remaining to be read in block
    bool eof;                                                       // Has the file reached eof?
} StorageFileReadDriverRemote;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_FILE_READ_DRIVER_REMOTE_TYPE                                                                          \
    StorageFileReadDriverRemote *
#define FUNCTION_LOG_STORAGE_FILE_READ_DRIVER_REMOTE_FORMAT(value, buffer, bufferSize)                                             \
    objToLog(value, "StorageFileReadDriverRemote", buffer, bufferSize)

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageFileReadDriverRemoteOpen(THIS_VOID)
{
    THIS(StorageFileReadDriverRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_FILE_READ_DRIVER_REMOTE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR);
        protocolCommandParamAdd(command, VARSTR(this->interface.name));
        protocolCommandParamAdd(command, VARBOOL(this->interface.ignoreMissing));

        result = varBool(protocolClientExecute(this->client, command, true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static size_t
storageFileReadDriverRemote(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(StorageFileReadDriverRemote);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_FILE_READ_DRIVER_REMOTE, this);
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
                    this->remaining = (size_t)storageDriverRemoteProtocolBlockSize(ioReadLine(protocolClientIoRead(this->client)));

                    if (this->remaining == 0)
                        this->eof = true;
                }
                MEM_CONTEXT_TEMP_END();
            }

            // Read if not eof
            if (!this->eof)
            {
                // If the buffer can contain all remaining bytes
                if (bufRemains(buffer) >= this->remaining)
                {
                    bufLimitSet(buffer, bufUsed(buffer) + this->remaining);
                    ioRead(protocolClientIoRead(this->client), buffer);
                    bufLimitClear(buffer);
                    this->remaining = 0;
                }
                // Else read what we can
                else
                    this->remaining -= ioRead(protocolClientIoRead(this->client), buffer);
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
storageFileReadDriverRemoteEof(THIS_VOID)
{
    THIS(StorageFileReadDriverRemote);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ_DRIVER_REMOTE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->eof);
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
StorageFileRead *
storageFileReadDriverRemoteNew(StorageDriverRemote *storage, ProtocolClient *client, const String *name, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, storage);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(client != NULL);
    ASSERT(name != NULL);

    StorageFileRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageFileReadDriverRemote")
    {
        StorageFileReadDriverRemote *driver = memNew(sizeof(StorageFileReadDriverRemote));
        driver->memContext = MEM_CONTEXT_NEW();

        driver->interface = (StorageFileReadInterface)
        {
            .type = STORAGE_DRIVER_REMOTE_TYPE_STR,
            .name = strDup(name),
            .ignoreMissing = ignoreMissing,

            .ioInterface = (IoReadInterface)
            {
                .eof = storageFileReadDriverRemoteEof,
                .open = storageFileReadDriverRemoteOpen,
                .read = storageFileReadDriverRemote,
            },
        };

        driver->storage = storage;
        driver->client = client;

        this = storageFileReadNew(driver, &driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_FILE_READ, this);
}
