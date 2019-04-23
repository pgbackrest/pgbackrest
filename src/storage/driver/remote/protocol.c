/***********************************************************************************************************************************
Remote Storage Protocol Handler
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "storage/driver/remote/protocol.h"
#include "storage/helper.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_EXISTS_STR,                  PROTOCOL_COMMAND_STORAGE_EXISTS);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_LIST_STR,                    PROTOCOL_COMMAND_STORAGE_LIST);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR,               PROTOCOL_COMMAND_STORAGE_OPEN_READ);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_OPEN_WRITE_STR,              PROTOCOL_COMMAND_STORAGE_OPEN_WRITE);

/***********************************************************************************************************************************
Regular expressions
***********************************************************************************************************************************/
STRING_STATIC(BLOCK_REG_EXP_STR,                                    PROTOCOL_BLOCK_HEADER "-1|[0-9]+");

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct
{
    MemContext *memContext;                                         // Mem context
    RegExp *blockRegExp;                                            // Regular expression to check block messages
} storageDriverRemoteProtocolLocal;

/***********************************************************************************************************************************
Process storage protocol requests
***********************************************************************************************************************************/
bool
storageDriverRemoteProtocol(const String *command, const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);

    // Determine which storage should be used (??? for now this is only repo)
    const Storage *storage = storageRepo();
    StorageInterface interface = storageInterface(storage);
    void *driver = storageDriver(storage);

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, PROTOCOL_COMMAND_STORAGE_EXISTS_STR))
        {
            protocolServerResponse(server, VARBOOL(             // The unusual line break is to make coverage happy -- not sure why
                interface.exists(driver, storagePathNP(storage, varStr(varLstGet(paramList, 0))))));
        }
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_LIST_STR))
        {
            protocolServerResponse(
                server,
                varNewVarLst(
                    varLstNewStrLst(
                        interface.list(
                            driver, storagePathNP(storage, varStr(varLstGet(paramList, 0))), varBool(varLstGet(paramList, 1)),
                            varStr(varLstGet(paramList, 2))))));
        }
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR))
        {
            // Create the read object
            IoRead *fileRead = storageFileReadIo(
                interface.newRead(
                    driver, storagePathNP(storage, varStr(varLstGet(paramList, 0))), varBool(varLstGet(paramList, 1))));

            // Check if the file exists
            bool exists = ioReadOpen(fileRead);
            protocolServerResponse(server, VARBOOL(exists));

            // Transfer the file if it exists
            if (exists)
            {
                Buffer *buffer = bufNew(ioBufferSize());

                // Write file out to protocol layer
                do
                {
                    ioRead(fileRead, buffer);

                    if (bufUsed(buffer) > 0)
                    {
                        ioWriteStrLine(protocolServerIoWrite(server), strNewFmt(PROTOCOL_BLOCK_HEADER "%zu", bufUsed(buffer)));
                        ioWrite(protocolServerIoWrite(server), buffer);
                        ioWriteFlush(protocolServerIoWrite(server));

                        bufUsedZero(buffer);
                    }
                }
                while (!ioReadEof(fileRead));

                // Write a zero block to show file is complete
                ioWriteLine(protocolServerIoWrite(server), BUFSTRDEF(PROTOCOL_BLOCK_HEADER "0"));
                ioWriteFlush(protocolServerIoWrite(server));
            }
        }
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_OPEN_WRITE_STR))
        {
            // Create the write object
            IoWrite *fileWrite = storageFileWriteIo(
                interface.newWrite(
                    driver, storagePathNP(storage, varStr(varLstGet(paramList, 0))), (mode_t)varUIntForce(varLstGet(paramList, 1)),
                    (mode_t)varUIntForce(varLstGet(paramList, 2)), varBool(varLstGet(paramList, 3)),
                    varBool(varLstGet(paramList, 4)), varBool(varLstGet(paramList, 5)), varBool(varLstGet(paramList, 6))));

            // Open file
            ioWriteOpen(fileWrite);
            protocolServerResponse(server, NULL);

            // Write data
            Buffer *buffer = bufNew(ioBufferSize());
            ssize_t remaining;

            do
            {
                // How much data is remaining to write?
                remaining = storageDriverRemoteProtocolBlockSize(ioReadLine(protocolServerIoRead(server)));

                // Write data
                if (remaining > 0)
                {
                    size_t bytesToCopy = (size_t)remaining;

                    do
                    {
                        if (bytesToCopy < bufSize(buffer))
                            bufLimitSet(buffer, bytesToCopy);

                        bytesToCopy -= ioRead(protocolServerIoRead(server), buffer);
                        ioWrite(fileWrite, buffer);

                        bufUsedZero(buffer);
                        bufLimitClear(buffer);
                    }
                    while (bytesToCopy > 0);
                }
                // Close when all data has been written
                else if (remaining == 0)
                {
                    ioWriteClose(fileWrite);
                }
                // Write was aborted so free the file
                else
                    ioWriteFree(fileWrite);
            }
            while (remaining > 0);

            protocolServerResponse(server, NULL);
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}

/***********************************************************************************************************************************
Get size of the next transfer block
***********************************************************************************************************************************/
ssize_t
storageDriverRemoteProtocolBlockSize(const String *message)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, message);
    FUNCTION_LOG_END();

    ASSERT(message != NULL);

    // Create block regular expression if it has not been created yet
    if (storageDriverRemoteProtocolLocal.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN("StorageDriverRemoteFileReadLocal")
            {
                storageDriverRemoteProtocolLocal.memContext = memContextCurrent();
                storageDriverRemoteProtocolLocal.blockRegExp = regExpNew(BLOCK_REG_EXP_STR);
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }

    // Validate the header block size message
    if (!regExpMatch(storageDriverRemoteProtocolLocal.blockRegExp, message))
        THROW_FMT(ProtocolError, "'%s' is not a valid block size message", strPtr(message));

    FUNCTION_LOG_RETURN(SSIZE, (ssize_t)cvtZToInt(strPtr(message) + sizeof(PROTOCOL_BLOCK_HEADER) - 1));
}
