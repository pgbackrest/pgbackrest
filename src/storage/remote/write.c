/***********************************************************************************************************************************
Remote Storage File write
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
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
    ProtocolClient *client;                                         // Protocol client to make requests with
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
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_OPEN_WRITE_STR);
        protocolCommandParamAdd(command, VARSTR(this->interface.name));
        protocolCommandParamAdd(command, VARUINT(this->interface.modeFile));
        protocolCommandParamAdd(command, VARUINT(this->interface.modePath));
        protocolCommandParamAdd(command, VARSTR(this->interface.user));
        protocolCommandParamAdd(command, VARSTR(this->interface.group));
        protocolCommandParamAdd(command, VARINT64(this->interface.timeModified));
        protocolCommandParamAdd(command, VARBOOL(this->interface.createPath));
        protocolCommandParamAdd(command, VARBOOL(this->interface.syncFile));
        protocolCommandParamAdd(command, VARBOOL(this->interface.syncPath));
        protocolCommandParamAdd(command, VARBOOL(this->interface.atomic));

        protocolClientExecute(this->client, command, false);

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
        protocolClientReadOutput(this->client, false);
        this->client = NULL;

        memContextCallbackClear(this->memContext);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Create a new file
***********************************************************************************************************************************/
StorageWrite *
storageWriteRemoteNew(
    StorageRemote *storage, ProtocolClient *client, const String *name, mode_t modeFile, mode_t modePath, const String *user,
    const String *group, time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_REMOTE, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(STRING, group);
        FUNCTION_LOG_PARAM(INT64, timeModified);
        FUNCTION_LOG_PARAM(BOOL, createPath);
        FUNCTION_LOG_PARAM(BOOL, syncFile);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
        FUNCTION_LOG_PARAM(BOOL, atomic);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(client != NULL);
    ASSERT(name != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    StorageWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageWriteRemote")
    {
        StorageWriteRemote *driver = memNew(sizeof(StorageWriteRemote));
        driver->memContext = MEM_CONTEXT_NEW();

        driver->interface = (StorageWriteInterface)
        {
            .type = STORAGE_REMOTE_TYPE_STR,
            .name = strDup(name),
            .atomic = atomic,
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
        };

        driver->storage = storage;
        driver->client = client;

        this = storageWriteNew(driver, &driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}
