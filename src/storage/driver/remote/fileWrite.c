/***********************************************************************************************************************************
Remote Storage File Write Driver
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/driver/remote/fileWrite.h"
#include "storage/driver/remote/protocol.h"
#include "storage/fileWrite.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageDriverRemoteFileWrite
{
    MemContext *memContext;
    StorageDriverRemote *storage;
    ProtocolClient *client;
    StorageFileWrite *interface;
    IoWrite *io;

    String *name;
    mode_t modeFile;
    mode_t modePath;
    bool createPath;
    bool syncFile;
    bool syncPath;
    bool atomic;
};

/***********************************************************************************************************************************
Create a new file
***********************************************************************************************************************************/
StorageDriverRemoteFileWrite *
storageDriverRemoteFileWriteNew(
    StorageDriverRemote *storage, ProtocolClient *client, const String *name, mode_t modeFile, mode_t modePath, bool createPath,
    bool syncFile, bool syncPath, bool atomic)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, createPath);
        FUNCTION_LOG_PARAM(BOOL, syncFile);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
        FUNCTION_LOG_PARAM(BOOL, atomic);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    StorageDriverRemoteFileWrite *this = NULL;

    // Create the file
    MEM_CONTEXT_NEW_BEGIN("StorageDriverRemoteFileWrite")
    {
        this = memNew(sizeof(StorageDriverRemoteFileWrite));
        this->memContext = MEM_CONTEXT_NEW();
        this->storage = storage;
        this->client = client;

        this->interface = storageFileWriteNewP(
            STORAGE_DRIVER_REMOTE_TYPE_STR, this, .atomic = (StorageFileWriteInterfaceAtomic)storageDriverRemoteFileWriteAtomic,
            .createPath = (StorageFileWriteInterfaceCreatePath)storageDriverRemoteFileWriteCreatePath,
            .io = (StorageFileWriteInterfaceIo)storageDriverRemoteFileWriteIo,
            .modeFile = (StorageFileWriteInterfaceModeFile)storageDriverRemoteFileWriteModeFile,
            .modePath = (StorageFileWriteInterfaceModePath)storageDriverRemoteFileWriteModePath,
            .name = (StorageFileWriteInterfaceName)storageDriverRemoteFileWriteName,
            .syncFile = (StorageFileWriteInterfaceSyncFile)storageDriverRemoteFileWriteSyncFile,
            .syncPath = (StorageFileWriteInterfaceSyncPath)storageDriverRemoteFileWriteSyncPath);

        this->io = ioWriteNewP(
            this, .close = (IoWriteInterfaceClose)storageDriverRemoteFileWriteClose,
            .open = (IoWriteInterfaceOpen)storageDriverRemoteFileWriteOpen,
            .write = (IoWriteInterfaceWrite)storageDriverRemoteFileWrite);

        this->name = strDup(name);
        this->modeFile = modeFile;
        this->modePath = modePath;
        this->createPath = createPath;
        this->syncFile = syncFile;
        this->syncPath = syncPath;
        this->atomic = atomic;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
void
storageDriverRemoteFileWriteOpen(StorageDriverRemoteFileWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_STORAGE_OPEN_WRITE_STR);
        protocolCommandParamAdd(command, varNewStr(this->name));
        protocolCommandParamAdd(command, varNewUInt64(this->modeFile));
        protocolCommandParamAdd(command, varNewUInt64(this->modePath));
        protocolCommandParamAdd(command, varNewBool(this->createPath));
        protocolCommandParamAdd(command, varNewBool(this->syncFile));
        protocolCommandParamAdd(command, varNewBool(this->syncPath));
        protocolCommandParamAdd(command, varNewBool(this->atomic));

        protocolClientExecute(this->client, command, false);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to the file
***********************************************************************************************************************************/
void
storageDriverRemoteFileWrite(StorageDriverRemoteFileWrite *this, const Buffer *buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    ioWriteLine(protocolClientIoWrite(this->client), strNewFmt(PROTOCOL_BLOCK_HEADER "%zu", bufUsed(buffer)));
    ioWrite(protocolClientIoWrite(this->client), buffer);
    ioWriteFlush(protocolClientIoWrite(this->client));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
void
storageDriverRemoteFileWriteClose(StorageDriverRemoteFileWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Close if the file has not already been closed
    if (this->client != NULL)
    {
        ioWriteLine(protocolClientIoWrite(this->client), strNew(PROTOCOL_BLOCK_HEADER "0"));
        ioWriteFlush(protocolClientIoWrite(this->client));
        protocolClientReadOutput(this->client, false);

        this->client = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Will the file be written atomically?
***********************************************************************************************************************************/
bool
storageDriverRemoteFileWriteAtomic(const StorageDriverRemoteFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->atomic);
}

/***********************************************************************************************************************************
Will the path be created for the file if it does not exist?
***********************************************************************************************************************************/
bool
storageDriverRemoteFileWriteCreatePath(const StorageDriverRemoteFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->createPath);
}

/***********************************************************************************************************************************
Get interface
***********************************************************************************************************************************/
StorageFileWrite *
storageDriverRemoteFileWriteInterface(const StorageDriverRemoteFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface);
}

/***********************************************************************************************************************************
Get I/O interface
***********************************************************************************************************************************/
IoWrite *
storageDriverRemoteFileWriteIo(const StorageDriverRemoteFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->io);
}

/***********************************************************************************************************************************
Mode for the file to be created
***********************************************************************************************************************************/
mode_t
storageDriverRemoteFileWriteModeFile(const StorageDriverRemoteFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->modeFile);
}

/***********************************************************************************************************************************
Mode for any paths that are created while writing the file
***********************************************************************************************************************************/
mode_t
storageDriverRemoteFileWriteModePath(const StorageDriverRemoteFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->modePath);
}

/***********************************************************************************************************************************
File name
***********************************************************************************************************************************/
const String *
storageDriverRemoteFileWriteName(const StorageDriverRemoteFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->name);
}

/***********************************************************************************************************************************
Will the file be synced after it is closed?
***********************************************************************************************************************************/
bool
storageDriverRemoteFileWriteSyncFile(const StorageDriverRemoteFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->syncFile);
}

/***********************************************************************************************************************************
Will the directory be synced to disk after the write is completed?
***********************************************************************************************************************************/
bool
storageDriverRemoteFileWriteSyncPath(const StorageDriverRemoteFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->syncPath);
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageDriverRemoteFileWriteFree(StorageDriverRemoteFileWrite *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_REMOTE_FILE_WRITE, this);
    FUNCTION_LOG_END();

    if (this != NULL)
    {
        memContextCallbackClear(this->memContext);

        // If freed without closing then notify the remote to close the file
        if (this->client != NULL)
        {
            ioWriteLine(protocolClientIoWrite(this->client), strNew(PROTOCOL_BLOCK_HEADER "-1"));
            ioWriteFlush(protocolClientIoWrite(this->client));
            protocolClientReadOutput(this->client, false);

            this->client = NULL;
        }

        memContextFree(this->memContext);
    }

    FUNCTION_LOG_RETURN_VOID();
}
